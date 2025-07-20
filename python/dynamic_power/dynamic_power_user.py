#!/usr/bin/env python3
import os
import sys
import time
import yaml
import argparse
import psutil
from systemd import journal

CONFIG_PATH = os.path.expanduser("~/.config/dynamic_power/config.yaml")
DEFAULT_POLL_INTERVAL = 5

def load_config():
    if not os.path.exists(CONFIG_PATH):
        journal.send("dynamic_power_user: Config file not found, using defaults")
        return {}, DEFAULT_POLL_INTERVAL
    try:
        with open(CONFIG_PATH, "r") as f:
            config = yaml.safe_load(f)
        poll_interval = config.get("general", {}).get("poll_interval", DEFAULT_POLL_INTERVAL)
        return config, poll_interval
    except Exception as e:
        journal.send(f"dynamic_power_user: Failed to load config: {e}")
        return {}, DEFAULT_POLL_INTERVAL

def scan_processes(overrides, seen, debug):
    current_processes = {p.name() for p in psutil.process_iter(['name'])}
    for override in overrides:
        name = override.get("process_name")
        if not name:
            continue
        found = name in current_processes
        if found and name not in seen:
            journal.send(f"dynamic_power_user: Matched override: {override.get('name')} (profile={override.get('active_profile')})")
            if debug:
                print(f"Matched override: {override.get('name')} (profile={override.get('active_profile')})")
            seen.add(name)
        elif not found and name in seen:
            seen.remove(name)
            if debug:
                print(f"Process '{name}' is no longer running.")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--debug", action="store_true", help="Enable verbose logging")
    args = parser.parse_args()

    config, poll_interval = load_config()
    overrides = config.get("process_overrides", [])
    seen = set()

    journal.send("dynamic_power_user: Starting monitor")

    try:
        while True:
            config, poll_interval = load_config()
            overrides = config.get("process_overrides", [])
            scan_processes(overrides, seen, args.debug)
            time.sleep(poll_interval)
    except KeyboardInterrupt:
        journal.send("dynamic_power_user: Exiting monitor")

if __name__ == "__main__":
    main()
