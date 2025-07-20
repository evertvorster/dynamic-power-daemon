#!/usr/bin/env python3

import os
import time
import yaml
import argparse
import psutil
from systemd import journal

CONFIG_PATH = os.path.expanduser("~/.config/dynamic_power/config.yaml")
TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power-user.yaml"

def load_config():
    if os.path.exists(CONFIG_PATH):
        path = CONFIG_PATH
    else:
        path = TEMPLATE_PATH

    with open(path, "r") as f:
        config = yaml.safe_load(f) or {}

    return config, path

def get_poll_interval(config):
    try:
        return int(config.get("general", {}).get("poll_interval", 5))
    except (ValueError, TypeError):
        return 5

def get_thresholds(config):
    thresholds = config.get("thresholds", {})
    return (
        float(thresholds.get("low", 1.0)),
        float(thresholds.get("high", 2.0)),
    )

def get_process_overrides(config):
    return config.get("process_overrides", [])

def send_thresholds_to_daemon(low, high, debug):
    try:
        import dbus
        bus = dbus.SystemBus()
        proxy = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
        iface = dbus.Interface(proxy, dbus_interface="org.dynamic_power.Daemon")
        result = iface.SetLoadThresholds(float(low), float(high))
        journal.send(f"dynamic_power_user: Sent thresholds to daemon: low={low}, high={high}, result={result}")
        if debug:
            print(f"Sent thresholds to daemon: low={low}, high={high}, result={result}")
    except Exception as e:
        journal.send(f"dynamic_power_user: Failed to send thresholds to daemon: {e}")

def scan_processes(overrides, state, debug):
    found_processes = set()
    running = {proc.name() for proc in psutil.process_iter(attrs=["name"])}

    for override in overrides:
        process = override.get("process")
        label = override.get("label", process)
        if not process:
            continue
        is_running = process in running
        if debug:
            print(f"Checking for process '{process}' -> {'FOUND' if is_running else 'not found'}")
        if is_running:
            found_processes.add(process)
            if process not in state["seen"]:
                state["seen"].add(process)
                journal.send(f"dynamic_power_user: Matched override: {label} (profile={override.get('profile')})")
                if debug:
                    print(f"Matched override: {label} (profile={override.get('profile')})")
        else:
            if process in state["seen"]:
                state["seen"].remove(process)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--debug", action="store_true", help="Enable verbose logging")
    args = parser.parse_args()

    journal.send("dynamic_power_user: Starting monitor")
    config, path = load_config()
    state = {"seen": set()}

    try:
        while True:
            config, _ = load_config()
            poll_interval = get_poll_interval(config)
            low, high = get_thresholds(config)
            send_thresholds_to_daemon(low, high, args.debug)
            overrides = get_process_overrides(config)
            scan_processes(overrides, state, args.debug)
            time.sleep(poll_interval)
    except KeyboardInterrupt:
        journal.send("dynamic_power_user: Exiting monitor")

if __name__ == "__main__":
    main()
