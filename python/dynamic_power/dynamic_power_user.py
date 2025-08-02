#!/usr/bin/env python3
import os
import sys
import logging
import time
import yaml
import psutil
import types
import dbus
from inotify_simple import INotify, flags
import signal
from systemd import journal
from setproctitle import setproctitle
from dynamic_power.config import is_debug_enabled

DEBUG = is_debug_enabled()
logging.debug("Debug mode in dynamic power user is enabled (via config)")
# ---------------------------------------------------------------------------
DEBUG = "--debug" in sys.argv
CONFIG_PATH   = os.path.expanduser("~/.config/dynamic_power/config.yaml")
TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power-user.yaml"

terminate                 = False
last_seen_processes       = set()
last_sent_profile         = None
last_sent_threshold       = None
threshold_override_active = False
active_profile_process    = None
last_manual_override      = None

# ---------------------------------------------------------------------------
# logging helpers
def log(msg: str) -> None:
    journal.send(f"dpu_user: {msg}")

def debug(msg: str) -> None:
    if DEBUG:
        journal.send(f"dpu_user (debug): {msg}")

def error(msg: str) -> None:
    """Always log errors to the journal."""
    journal.send(f"dpu_user (error): {msg}")

# ---------------------------------------------------------------------------
# config helpers
def load_template():
    with open(TEMPLATE_PATH, "r") as f:
        return yaml.safe_load(f) or {}

def load_config():
    if not os.path.exists(CONFIG_PATH):
        logging.info(f"Config file not found at {CONFIG_PATH}, using template.")
        return load_template()
    with open(CONFIG_PATH, "r") as f:
        cfg = yaml.safe_load(f) or {}
    logging.info("Loaded user config.")
    return cfg

# ---------------------------------------------------------------------------
# DBus helpers
def send_thresholds(bus, low: float, high: float) -> None:
    global last_sent_threshold
    if (low, high) == last_sent_threshold and not DEBUG:
        return
    try:
        daemon = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
        iface  = dbus.Interface(daemon, "org.dynamic_power.Daemon")
        iface.SetLoadThresholds(float(low), float(high))
        last_sent_threshold = (low, high)
        logging.debug(f"Sent thresholds: low={low}, high={high}")
    except Exception as e:
        logging.debug(f"[send_thresholds] {e}")

def send_profile(bus, profile: str) -> None:
    """Forward a power‑profilesd profile to the root daemon."""
    logging.debug("[dynamic_power_user][send_profile] sending profile override: %s", profile)
    global last_sent_profile
    if profile == last_sent_profile and not DEBUG:
        return
    try:
        daemon = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
        iface  = dbus.Interface(daemon, "org.dynamic_power.Daemon")
        iface.SetProfile(profile)
        last_sent_profile = profile
        logging.info(f"Sent profile override: {profile}")
        try:
            session_bus = dbus.SessionBus()
            helper = session_bus.get_object("org.dynamic_power.UserBus", "/")
            iface = dbus.Interface(helper, "org.dynamic_power.UserBus")
            iface.UpdateDaemonState(profile)
        except Exception as e:
            logging.debug(f"[send_profile] Failed to update GUI: {e}")
    except Exception as e:
        logging.info(f"[send_profile] {e}")

# ---------------------------------------------------------------------------
PROFILE_ALIASES = {
    "powersave":    "power-saver",
    "power_save":   "power-saver",
    "power-saver":  "power-saver",
    "balanced":     "balanced",
    "performance":  "performance",
    "quiet":        "power-saver",
}

def normalize_profile(name: str) -> str:
    return PROFILE_ALIASES.get(name.lower(), name)

# ---------------------------------------------------------------------------
def apply_process_policy(bus, name: str, policy: dict, high_th: float) -> None:
    """Enforce the policy for the *highest‑priority* matched process."""
    global threshold_override_active, active_profile_process

    profile = (policy.get("active_profile") or "").lower()

    # special “responsive” mode – blocks powersave by setting low threshold 0
    if profile == "responsive":
        threshold_override_active = True
        send_thresholds(bus, 0, high_th)
        logging.debug(f"{name} -> prevent_powersave (responsive)")
        return

    if profile:
        threshold_override_active = False
        active_profile_process = name
        send_profile(bus, normalize_profile(profile))
        logging.debug(f"{name} -> active_profile={profile}")
        return

# ---------------------------------------------------------------------------
def check_processes(bus, process_overrides, high_th: float) -> None:
    """Match running processes against the overrides list and act."""
    global last_seen_processes, threshold_override_active, active_profile_process

    # honour manual override first
    logging.debug("[dynamic_power_user][check_processes] called hidden message")
    try:
        session_bus = dbus.SessionBus()
        helper = session_bus.get_object("org.dynamic_power.UserBus", "/")
        iface = dbus.Interface(helper, "org.dynamic_power.UserBus")
        mode = iface.GetUserOverride()
    except Exception as e:
        logging.info(f"[read_override_from_dbus] {e}")
        mode = "Dynamic"

    mode = str(mode)
    logging.debug("[dynamic_power_user][check_processes] GetUserOverride() returned: %r", mode)
    global last_manual_override
    if mode != last_manual_override:
        logging.debug("[dynamic_power_user][check_processes] New override received: %s", mode)
    last_manual_override = mode

    if mode == "Dynamic":
        logging.debug("[dynamic_power_user][check_processes] Matched Dynamic")
        if last_sent_profile:
            send_profile(bus, "")
            logging.debug("[dynamic_power_user][check_processes] Cleared previous manual override")
        if threshold_override_active:
            threshold_override_active = False
            logging.info("[dynamic_power_user][check_processes] Cleared threshold override")
        return  # manual override active, skip process matching

    if mode == "Inhibit Powersave":
        logging.debug("[dynamic_power_user][check_processes] Matched Inhibit Powersave")
        send_thresholds(bus, 0, high_th)
        threshold_override_active = True
        logging.debug("Override → Inhibit Powersave")
        return
    elif mode in ["Performance", "Balanced", "Powersave"]:
        logging.debug("[dynamic_power_user][check_processes] Matched a direct mode")
        send_profile(bus, normalize_profile(mode))
        logging.debug(f"Override → {mode}")
        return
    else:
        # ensure any previous override is cleared
        if active_profile_process:
            send_profile(bus, "")
            active_profile_process = None

    # ------------------------------------------------------------------
    # process matching: only check processes owned by current user
    user_uid = os.getuid()

    # Accept both list and dict format from config
    if isinstance(process_overrides, list):
        proc_map = { (entry.get("process_name") or "").lower(): entry for entry in process_overrides }
    else:
        proc_map = { (k or "").lower(): v for k, v in process_overrides.items() }

    match_names = set(proc_map.keys())
    running = set()

    matches = []
    for proc in psutil.process_iter(['name', 'uids']):
        try:
            if not proc.info or proc.info['uids'].real != user_uid:
                continue
            name = (proc.info.get("name") or "").lower()
            if name in match_names:
                running.add(name)
                policy = proc_map[name]
                prio = policy.get("priority", 0)
                matches.append((prio, name, policy))
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            continue

    if matches:
        matches.sort(reverse=True)
        selected_prio, selected_name, selected_policy = matches[0]
        try:
            session_bus = dbus.SessionBus()
            helper = session_bus.get_object("org.dynamic_power.UserBus", "/")
            iface = dbus.Interface(helper, "org.dynamic_power.UserBus")

            iface.GetMetrics()  # sanity ping to ensure connection

            iface.UpdateProcessMatches([
                {
                    "process_name": name,
                    "priority": prio,
                    "active": (name == selected_name),
                }
                for prio, name, _ in matches
            ])
            iface.SetUserOverride(selected_policy.get("mode", "Dynamic").capitalize())
            logging.debug("Sent process matches via DBus to UserBus")
        except Exception as e:
            logging.info(f"[dbus_send_matches] {e}")

        apply_process_policy(bus, selected_name, selected_policy, high_th)
    #
    else:
        try:
            session_bus = dbus.SessionBus()
            helper = session_bus.get_object("org.dynamic_power.UserBus", "/")
            iface = dbus.Interface(helper, "org.dynamic_power.UserBus")
            iface.UpdateProcessMatches([])  # Send empty list to clear matches
            if last_manual_override == "Dynamic":
                iface.SetUserOverride("Dynamic")
                logging.debug("[dynamic_power_user][check_processes] Cleared manual override")

            logging.debug("Sent empty process match list via DBus")
        except Exception as e:
            logging.info(f"[dbus_send_empty_matches] {e}")
        if threshold_override_active:
            threshold_override_active = False
            logging.info("No override process active. Resetting thresholds to config.")
        last_seen_processes &= running


# ---------------------------------------------------------------------------
def handle_sigint(signum, frame):
    global terminate
    logging.info("Caught SIGINT, shutting down...")
    terminate = True

# ---------------------------------------------------------------------------
def main():
    global terminate, threshold_override_active

    setproctitle("dynamic_power_user")
    signal.signal(signal.SIGINT, handle_sigint)

    bus = dbus.SystemBus()
    config = load_config()

    inotify       = INotify()
    watch_flags   = flags.MODIFY
    inotify.add_watch(CONFIG_PATH, watch_flags)
    last_mtime    = os.path.getmtime(CONFIG_PATH) if os.path.exists(CONFIG_PATH) else 0

    poll_interval     = config.get("general", {}).get("poll_interval", 5)
    thresholds        = config.get("power",   {}).get("load_thresholds", {})
    process_overrides = config.get("process_overrides", {})

    logging.info("dynamic_power_user started.")

    while not terminate:
        try:
            # hot‑reload config when mtime changes (fallback polling)
            if os.path.exists(CONFIG_PATH):
                mtime = os.path.getmtime(CONFIG_PATH)
                if mtime != last_mtime:
                    config = load_config()
                    poll_interval     = config.get("general", {}).get("poll_interval", 5)
                    thresholds        = config.get("power",   {}).get("load_thresholds", {})
                    process_overrides = config.get("process_overrides", {})
                    last_mtime        = mtime
                    logging.debug("Reloaded config due to mtime change.")

            check_processes(bus, process_overrides, thresholds.get("high", 2.0))
            logging.debug("[dynamic_power_user][main_loop] tick")

            if not threshold_override_active:
                send_thresholds(bus,
                                thresholds.get("low", 1.0),
                                thresholds.get("high", 2.0))

            # event‑driven reload via inotify (no need to update last_mtime)
            for event in inotify.read(timeout=0):
                if flags.MODIFY in flags.from_mask(event.mask):
                    config = load_config()
                    poll_interval     = config.get("general", {}).get("poll_interval", 5)
                    thresholds        = config.get("power",   {}).get("load_thresholds", {})
                    process_overrides = config.get("process_overrides", {})
                    logging.debug("Reloaded config via inotify.")

            time.sleep(poll_interval)

        except Exception as e:
            logging.info(f"[main_loop] {e}")
            time.sleep(poll_interval)

    logging.info("dynamic_power_user exited cleanly.")

# ---------------------------------------------------------------------------
if __name__ == "__main__":
    main()
