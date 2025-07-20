
#!/usr/bin/env python3
import os
import time
import yaml
import psutil
import dbus
import argparse
from pathlib import Path
from systemd import journal

USER_CONFIG_PATH = Path.home() / ".config/dynamic_power/config.yaml"
SYSTEM_TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power.yaml"

class ConfigManager:
    def __init__(self):
        self.config_path = USER_CONFIG_PATH if USER_CONFIG_PATH.exists() else SYSTEM_TEMPLATE_PATH
        self.data = {}
        self.last_mtime = 0
        self.load()

    def load(self):
        try:
            with open(self.config_path, "r") as f:
                self.data = yaml.safe_load(f) or {}
            self.last_mtime = os.path.getmtime(self.config_path)
            journal.send(f"dynamic_power_user: Config loaded from: {self.config_path}")
        except Exception as e:
            journal.send(f"dynamic_power_user: Failed to load config: {e}")
            self.data = {}

    def reload_if_needed(self):
        try:
            mtime = os.path.getmtime(self.config_path)
            if mtime > self.last_mtime:
                journal.send("dynamic_power_user: Detected config change, reloading...")
                self.load()
        except FileNotFoundError:
            pass

    def get_thresholds(self):
        thresholds = self.data.get("power", {}).get("load_thresholds", {})
        return (
            float(thresholds.get("low", 1.0)),
            float(thresholds.get("high", 2.0)),
        )

    def get_poll_interval(self):
        return int(self.data.get("general", {}).get("poll_interval", 5))

    def get_process_overrides(self):
        return self.data.get("process_overrides", [])

def send_thresholds_to_daemon(low, high, debug):
    try:
        bus = dbus.SystemBus()
        obj = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
        iface = dbus.Interface(obj, "org.dynamic_power.Daemon")
        result = iface.SetLoadThresholds(dbus.Double(low), dbus.Double(high))
        journal.send(f"dynamic_power_user: Sent thresholds to daemon: low={low}, high={high}, result={result}")
    except Exception as e:
        journal.send(f"dynamic_power_user: Failed to send thresholds: {e}")
        if debug:
            print(f"Exception: {e}")

def scan_processes(process_overrides, state, debug):
    running = list(psutil.process_iter(['name']))
    running_names = {p.info['name'] for p in running if p.info['name']}

    if debug:
        journal.send(f"dynamic_power_user: Running processes: {', '.join(sorted(running_names))}")

    for override in process_overrides:
        name = override.get("process_name")
        matched = name in running_names
        if matched and name not in state["seen"]:
            journal.send(f"dynamic_power_user: Checking for process '{name}' -> FOUND")
            journal.send(f"dynamic_power_user: Matched override: {override.get('name')} (profile={override.get('active_profile')})")
            state["seen"].add(name)
        elif not matched and name in state["seen"]:
            state["seen"].remove(name)
            journal.send(f"dynamic_power_user: Process '{name}' no longer detected")

        if debug and not matched:
            journal.send(f"dynamic_power_user: Checking for process '{name}' -> not found")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--debug", action="store_true", help="Enable verbose logging")
    args = parser.parse_args()

    journal.send("dynamic_power_user: Starting monitor")
    config = ConfigManager()
    state = {"seen": set()}

    try:
        while True:
            config.reload_if_needed()

            low, high = config.get_thresholds()
            send_thresholds_to_daemon(low, high, args.debug)

            overrides = config.get_process_overrides()
            scan_processes(overrides, state, args.debug)

            time.sleep(config.get_poll_interval())

    except KeyboardInterrupt:
        journal.send("dynamic_power_user: Exiting monitor")

if __name__ == "__main__":
    main()
