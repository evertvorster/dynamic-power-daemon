#!/usr/bin/env python3
import os
import sys
import time
import yaml
import psutil
import dbus
import signal
from systemd import journal
from setproctitle import setproctitle

DEBUG = "--debug" in sys.argv
CONFIG_PATH = os.path.expanduser("~/.config/dynamic_power/config.yaml")
TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power-user.yaml"

terminate = False
last_seen_processes = set()
last_sent_profile = None
last_sent_threshold = None
threshold_override_active = False
active_profile_process = None

def log(msg):
    journal.send(f"dpu_user: {msg}")

def debug(msg):
    if DEBUG:
        journal.send(f"dpu_user (debug): {msg}")

def load_config():
    if not os.path.exists(CONFIG_PATH):
        debug(f"Config file not found at {CONFIG_PATH}, using template.")
        return load_template()
    with open(CONFIG_PATH, "r") as f:
        cfg = yaml.safe_load(f) or {}
    debug("Loaded user config.")
    return cfg

def load_template():
    with open(TEMPLATE_PATH, "r") as f:
        return yaml.safe_load(f) or {}

def send_thresholds(bus, low, high):
    global last_sent_threshold
    if (low, high) == last_sent_threshold and not DEBUG:
        return
    try:
        daemon = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
        iface = dbus.Interface(daemon, "org.dynamic_power.Daemon")
        iface.SetLoadThresholds(float(low), float(high))
        last_sent_threshold = (low, high)
        debug(f"Sent thresholds: low={low}, high={high}")
    except Exception as e:
        if DEBUG:
            journal.send(f"dpu_user (error): Failed to send thresholds - {e}")

def send_profile(bus, profile):
    global last_sent_profile
    if profile == last_sent_profile and not DEBUG:
        return
    try:
        daemon = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
        iface = dbus.Interface(daemon, "org.dynamic_power.Daemon")
        iface.SetProfile(profile)
        last_sent_profile = profile
        debug(f"Sent profile override: {profile}")
    except Exception as e:
        if DEBUG:
            journal.send(f"dpu_user (error): Failed to send profile override - {e}")

def apply_process_policy(bus, name, policy, high_th):
    global threshold_override_active, active_profile_process
    if "active_profile" in policy:
        threshold_override_active = False
        active_profile_process = name
        send_profile(bus, policy["active_profile"])
        if DEBUG or name not in last_seen_processes:
            journal.send(f"dpu_user: {name} -> active_profile={policy['active_profile']}")
    elif policy.get("prevent_powersave", False):
        threshold_override_active = True
        send_thresholds(bus, 0, high_th)
        if DEBUG or name not in last_seen_processes:
            journal.send(f"dpu_user: {name} -> prevent_powersave=true")

def check_processes(bus, process_overrides, high_th):
    global last_seen_processes, threshold_override_active, active_profile_process
    running = set(p.info["name"] for p in psutil.process_iter(attrs=["name"]))

    if isinstance(process_overrides, list):
        proc_map = {entry.get("process_name"): entry for entry in process_overrides}
    else:
        proc_map = process_overrides

    matches = []
    for name, policy in proc_map.items():
        if name in running:
            prio = policy.get("priority", 0)
            matches.append((prio, name, policy))
            last_seen_processes.add(name)

    if matches:
        matches.sort(reverse=True)
        _, name, policy = matches[0]
        apply_process_policy(bus, name, policy, high_th)
    else:
        if active_profile_process:
            send_profile(bus, "")
            debug(f"Clearing active profile override from: {active_profile_process}")
            active_profile_process = None

        if threshold_override_active:
            threshold_override_active = False
            debug("No override process active. Resetting thresholds to config.")

    for proc in list(last_seen_processes):
        if proc not in running:
            last_seen_processes.remove(proc)
            if DEBUG:
                journal.send(f"dpu_user (debug): Process {proc} no longer running.")

def handle_sigint(signum, frame):
    global terminate
    log("Caught SIGINT, shutting down...")
    terminate = True


def write_state_file(profile, thresholds):
    try:
        uid = os.getuid()
        state_file = f"/run/user/{uid}/dynamic_power_state.yaml"
        state = {
            "active_profile": profile,
            "thresholds": {
                "low": thresholds.get("low", 1.0),
                "high": thresholds.get("high", 2.0)
            },
            "timestamp": time.time()
        }
        os.makedirs(os.path.dirname(state_file), exist_ok=True)
        with open(state_file, "w") as f:
            yaml.dump(state, f)
        if DEBUG:
            journal.send("dpu_user (debug): Wrote state file.")
    except Exception as e:
        if DEBUG:
            journal.send(f"dpu_user (error): Failed to write state file - {e}")
def main():
    global terminate, threshold_override_active

    setproctitle("dynamic_power_user")
    signal.signal(signal.SIGINT, handle_sigint)

    bus = dbus.SystemBus()
    last_mtime = 0
    config = load_config()
    last_mtime = os.path.getmtime(CONFIG_PATH) if os.path.exists(CONFIG_PATH) else 0

    poll_interval = config.get("general", {}).get("poll_interval", 5)
    thresholds = config.get("power", {}).get("load_thresholds", {})
    process_overrides = config.get("process_overrides", {})

    log("dynamic_power_user started.")

    while not terminate:
        try:
            if os.path.exists(CONFIG_PATH):
                mtime = os.path.getmtime(CONFIG_PATH)
                if mtime != last_mtime:
                    config = load_config()
                    poll_interval = config.get("general", {}).get("poll_interval", 5)
                    thresholds = config.get("power", {}).get("load_thresholds", {})
                    process_overrides = config.get("process_overrides", {})
                    last_mtime = mtime
                    debug("Reloaded config due to change.")

            check_processes(bus, process_overrides, thresholds.get("high", 2.0))

            if not threshold_override_active:
                send_thresholds(bus, thresholds.get("low", 1.0), thresholds.get("high", 2.0))

            write_state_file(last_sent_profile, thresholds)
            time.sleep(poll_interval)
        except Exception as e:
            if DEBUG:
                journal.send(f"dpu_user (error): {e}")
            time.sleep(poll_interval)

    log("dynamic_power_user exited cleanly.")

if __name__ == "__main__":
    main()
