#!/usr/bin/env python3
import os
import sys
import time
import yaml
import psutil
import dbus
from inotify_simple import INotify, flags
import signal
import getpass
from systemd import journal
from setproctitle import setproctitle

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

def load_config():
    if not os.path.exists(CONFIG_PATH):
        return load_template()
    with open(CONFIG_PATH, "r") as f:
        cfg = yaml.safe_load(f) or {}
    return cfg

def load_template():
    with open(TEMPLATE_PATH, "r") as f:
        return yaml.safe_load(f) or {}

def send_thresholds(bus, low, high):
    global last_sent_threshold
    if (low, high) == last_sent_threshold:
        return
    daemon = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
    iface = dbus.Interface(daemon, "org.dynamic_power.Daemon")
    iface.SetLoadThresholds(float(low), float(high))
    last_sent_threshold = (low, high)
def send_profile(bus, profile):
    global last_sent_profile
    if profile == last_sent_profile:
        return
    daemon = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
    iface = dbus.Interface(daemon, "org.dynamic_power.Daemon")
    iface.SetProfile(profile)
    last_sent_profile = profile
def normalize_profile(name):
    aliases = {
        "Powersave": "power-saver",
        "Balanced": "balanced",
        "Performance": "performance"
    }
    return aliases.get(name, name)
def apply_process_policy(bus, name, policy, high_th):
    """Apply per‑process power policy.
    • "responsive" → inhibit powersave by lowering the *low* threshold to 0.
    • any other recognised profile string is forwarded unchanged.
    """
    global threshold_override_active, active_profile_process
    profile = (policy.get("active_profile") or "").lower()

    if profile == "responsive":
        threshold_override_active = True
        send_thresholds(bus, 0, high_th)  # prevent entering powersave
        return

    if profile:
        threshold_override_active = False
        active_profile_process = name
        send_profile(bus, profile)
def check_processes(bus, process_overrides, high_th):
    global last_seen_processes, threshold_override_active, active_profile_process
    override = read_control_override()
    mode = override.get("manual_override", "Dynamic")
    if mode == "Dynamic":
        send_profile(bus, "")
    elif mode in ["Inhibit Powersave", "Responsive"]:
        send_thresholds(bus, 0, high_th)
        threshold_override_active = True
        return
    elif mode in ["Performance", "Balanced", "Powersave"]:
        send_profile(bus, normalize_profile(mode))
        return

    mode = override.get("manual_override", "Dynamic")
    if mode == "Inhibit Powersave":
        send_thresholds(bus, 0, thresholds.get("high", 2.0))
    elif mode in ["Performance", "Balanced", "Powersave"]:
        send_profile(bus, mode)
    running = set(p.info["name"] for p in psutil.process_iter(attrs=["name"]))

    if isinstance(process_overrides, list):
        proc_map = {entry.get("process_name"): entry for entry in process_overrides}
    else:
        # Remove matches file if it exists
        uid = os.getuid()
        match_file = f"/run/user/{uid}/dynamic_power_matches.yaml"
        if os.path.exists(match_file):
            os.remove(match_file)
        proc_map = process_overrides

    matches = []
    # Always try to remove old matches file, even if we’ll re-create it
    uid = os.getuid()
    match_file = f"/run/user/{uid}/dynamic_power_matches.yaml"
    if os.path.exists(match_file):
        os.remove(match_file)
    for name, policy in proc_map.items():
        if name in running:
            prio = policy.get("priority", 0)
            matches.append((prio, name, policy))
            last_seen_processes.add(name)

    if matches:
        # Write match information to /run/user/{uid}/dynamic_power_matches.yaml
        uid = os.getuid()
        match_file = f"/run/user/{uid}/dynamic_power_matches.yaml"
        match_info = {
            "matched_processes": [
                {
                    "process_name": name,
                    "priority": prio,
                    "active": (name == matches[0][1])
                } for prio, name, _ in matches
            ],
            "timestamp": time.time()
        }
        os.makedirs(os.path.dirname(match_file), exist_ok=True)
        with open(match_file, "w") as f:
            yaml.dump(match_info, f)
        matches.sort(reverse=True)
        _, name, policy = matches[0]
        apply_process_policy(bus, name, policy, high_th)
    else:
        if active_profile_process:
            send_profile(bus, "")
            active_profile_process = None

        if threshold_override_active:
            threshold_override_active = False

    for proc in list(last_seen_processes):
        if proc not in running:
            last_seen_processes.remove(proc)
def handle_sigint(signum, frame):
    global terminate
    log("Caught SIGINT, shutting down...")
    terminate = True


def write_state_file(profile, thresholds):
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
def read_control_override():
    uid = os.getuid()
    path = f"/run/user/{uid}/dynamic_power_control.yaml"
    if os.path.exists(path):
        with open(path, "r") as f:
            return yaml.safe_load(f)
    return {}
def main():
    global terminate, threshold_override_active

    setproctitle("dynamic_power_user")
    signal.signal(signal.SIGINT, handle_sigint)

    bus = dbus.SystemBus()
    last_mtime = 0
    config = load_config()
    inotify = INotify()
    watch_flags = flags.MODIFY
    wd = inotify.add_watch(CONFIG_PATH, watch_flags)
    last_mtime = os.path.getmtime(CONFIG_PATH) if os.path.exists(CONFIG_PATH) else 0

    poll_interval = config.get("general", {}).get("poll_interval", 5)
    thresholds = config.get("power", {}).get("load_thresholds", {})
    process_overrides = config.get("process_overrides", {})

    log("dynamic_power_user started.")

    while not terminate:
        if os.path.exists(CONFIG_PATH):
            mtime = os.path.getmtime(CONFIG_PATH)
            if mtime != last_mtime:
                config = load_config()
                poll_interval = config.get("general", {}).get("poll_interval", 5)
                thresholds = config.get("power", {}).get("load_thresholds", {})
                process_overrides = config.get("process_overrides", {})
                last_mtime = mtime

        check_processes(bus, process_overrides, thresholds.get("high", 2.0))

        if not threshold_override_active:
            send_thresholds(bus, thresholds.get("low", 1.0), thresholds.get("high", 2.0))

        write_state_file(last_sent_profile, thresholds)

        for event in inotify.read(timeout=0):
            if flags.MODIFY in flags.from_mask(event.mask):
                config = load_config()
        time.sleep(poll_interval)
    log("dynamic_power_user exited cleanly.")

if __name__ == "__main__":
    main()