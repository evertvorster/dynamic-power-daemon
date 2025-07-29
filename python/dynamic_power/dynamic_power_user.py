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
        debug(f"Config file not found at {CONFIG_PATH}, using template.")
        return load_template()
    with open(CONFIG_PATH, "r") as f:
        cfg = yaml.safe_load(f) or {}
    debug("Loaded user config.")
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
        debug(f"Sent thresholds: low={low}, high={high}")
    except Exception as e:
        error(f"[send_thresholds] {e}")

def send_profile(bus, profile: str) -> None:
    """Forward a power‑profilesd profile to the root daemon."""
    global last_sent_profile
    if profile == last_sent_profile and not DEBUG:
        return
    try:
        daemon = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
        iface  = dbus.Interface(daemon, "org.dynamic_power.Daemon")
        iface.SetProfile(profile)
        last_sent_profile = profile
        debug(f"Sent profile override: {profile}")
    except Exception as e:
        error(f"[send_profile] {e}")

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
        debug(f"{name} -> prevent_powersave (responsive)")
        return

    if profile:
        threshold_override_active = False
        active_profile_process = name
        send_profile(bus, normalize_profile(profile))
        debug(f"{name} -> active_profile={profile}")
        return

# ---------------------------------------------------------------------------
def read_control_override() -> dict:
    try:
        uid  = os.getuid()
        path = f"/run/user/{uid}/dynamic_power_control.yaml"
        if os.path.exists(path):
            with open(path, "r") as f:
                return yaml.safe_load(f) or {}
    except Exception as e:
        error(f"[read_control_override] {e}")
    return {}

# ---------------------------------------------------------------------------
def write_state_file(profile: str, thresholds: dict) -> None:
    try:
        uid        = os.getuid()
        state_file = f"/run/user/{uid}/dynamic_power_state.yaml"
        state = {
            "active_profile": profile,
            "thresholds": {
                "low":  thresholds.get("low", 1.0),
                "high": thresholds.get("high", 2.0),
            },
            "timestamp": time.time(),
        }
        os.makedirs(os.path.dirname(state_file), exist_ok=True)
        with open(state_file, "w") as f:
            yaml.dump(state, f)
    except Exception as e:
        error(f"[write_state_file] {e}")

# ---------------------------------------------------------------------------
def check_processes(bus, process_overrides, high_th: float) -> None:
    """Match running processes against the overrides list and act."""
    global last_seen_processes, threshold_override_active, active_profile_process

    # honour manual override first
    override = read_control_override()
    mode = override.get("manual_override", "Dynamic")

    if mode == "Inhibit Powersave":
        send_thresholds(bus, 0, high_th)
        threshold_override_active = True
        debug("Override → Inhibit Powersave")
        return
    elif mode in ["Performance", "Balanced", "Powersave"]:
        send_profile(bus, normalize_profile(mode))
        debug(f"Override → {mode}")
        return
    else:
        # ensure any previous override is cleared
        if active_profile_process:
            send_profile(bus, "")
            active_profile_process = None

    # ------------------------------------------------------------------
    # process matching
    running = set((p.info.get("name") or "").lower() for p in psutil.process_iter(attrs=["name"]))
    if isinstance(process_overrides, list):
        proc_map = { (entry.get("process_name") or "").lower(): entry for entry in process_overrides }
    else:
        proc_map = { (k or "").lower(): v for k, v in process_overrides.items() }

    matches = []
    for proc_name, policy in proc_map.items():
        if proc_name and proc_name in running:
            prio = policy.get("priority", 0)
            matches.append((prio, proc_name, policy))
            last_seen_processes.add(proc_name)

    uid        = os.getuid()
    match_file = f"/run/user/{uid}/dynamic_power_matches.yaml"

    # remove any stale file; will recreate below if needed
    try:
        if os.path.exists(match_file):
            os.remove(match_file)
    except Exception as e:
        error(f"[matches_file_remove] {e}")

    if matches:
        # select highest‑priority match (max prio)
        matches.sort(reverse=True)
        selected_prio, selected_name, selected_policy = matches[0]

        # write matches file for the UI
        try:
            match_info = {
                "matched_processes": [
                    {
                        "process_name": name,
                        "priority": prio,
                        "active": (name == selected_name),
                    }
                    for prio, name, _ in matches
                ],
                "timestamp": time.time(),
            }
            os.makedirs(os.path.dirname(match_file), exist_ok=True)
            with open(match_file, "w") as f:
                yaml.dump(match_info, f)
        except Exception as e:
            error(f"[matches_file_write] {e}")

        apply_process_policy(bus, selected_name, selected_policy, high_th)
    else:
        # no matches – reset threshold override if it was driven by a process
        if threshold_override_active:
            threshold_override_active = False
            debug("No override process active. Resetting thresholds to config.")

        # clean out last_seen_processes set
        last_seen_processes &= running

# ---------------------------------------------------------------------------
def handle_sigint(signum, frame):
    global terminate
    log("Caught SIGINT, shutting down...")
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

    log("dynamic_power_user started.")

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
                    debug("Reloaded config due to mtime change.")

            check_processes(bus, process_overrides, thresholds.get("high", 2.0))

            if not threshold_override_active:
                send_thresholds(bus,
                                thresholds.get("low", 1.0),
                                thresholds.get("high", 2.0))

            write_state_file(last_sent_profile, thresholds)

            # event‑driven reload via inotify (no need to update last_mtime)
            for event in inotify.read(timeout=0):
                if flags.MODIFY in flags.from_mask(event.mask):
                    config = load_config()
                    poll_interval     = config.get("general", {}).get("poll_interval", 5)
                    thresholds        = config.get("power",   {}).get("load_thresholds", {})
                    process_overrides = config.get("process_overrides", {})
                    debug("Reloaded config via inotify.")

            time.sleep(poll_interval)

        except Exception as e:
            error(f"[main_loop] {e}")
            time.sleep(poll_interval)

    log("dynamic_power_user exited cleanly.")

# ---------------------------------------------------------------------------
if __name__ == "__main__":
    main()
