#!/usr/bin/env python3

import os
import time
import argparse
import yaml
import psutil
from systemd import journal

CONFIG_PATH = os.path.expanduser("~/.config/dynamic_power/config.yaml")
DEFAULT_TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power-user.yaml"

class ConfigManager:
    def __init__(self):
        self.last_mtime = None
        self.config = {}
        self.load_config()

    def load_config(self):
        path = CONFIG_PATH if os.path.exists(CONFIG_PATH) else DEFAULT_TEMPLATE_PATH
        try:
            with open(path, "r") as f:
                self.config = yaml.safe_load(f)
                self.last_mtime = os.path.getmtime(path)
                journal.send(f"dynamic_power_user: Config loaded from: {path}")
        except Exception as e:
            journal.send(f"dynamic_power_user: Failed to load config from {path}: {e}")
            self.config = {}

    def reload_if_needed(self):
        path = CONFIG_PATH
        try:
            current_mtime = os.path.getmtime(path)
            if current_mtime != self.last_mtime:
                self.load_config()
        except Exception:
            pass

    def get_thresholds(self):
        try:
            thresholds = self.config.get("power", {}).get("load_thresholds", {})
            low = float(thresholds.get("low", 1.0))
            high = float(thresholds.get("high", 2.0))
            return low, high
        except Exception:
            return 1.0, 2.0

    def get_poll_interval(self):
        try:
            return float(self.config.get("general", {}).get("poll_interval", 5.0))
        except Exception:
            return 5.0

    def get_process_overrides(self):
        return self.config.get("process_overrides", [])

def send_thresholds_to_daemon(low, high, debug=False):
    import dbus
    try:
        bus = dbus.SystemBus()
        obj = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
        iface = dbus.Interface(obj, "org.dynamic_power.Daemon")
        result = iface.SetLoadThresholds(float(low), float(high))
        if debug:
            journal.send(f"dynamic_power_user: Sent thresholds to daemon: low={low}, high={high}, result={result}")
    except Exception as e:
        if debug:
            journal.send(f"dynamic_power_user: Failed to send thresholds: {e}")

def scan_processes(process_overrides, state, debug=False):
    matched = False
    running = set(p.name() for p in psutil.process_iter(attrs=["name"]))
    for override in process_overrides:
        name = override.get("process_name")
        if not name:
            continue
        found = any(name in proc for proc in running)
        if found:
            if name not in state["seen"]:
                msg = f"dynamic_power_user: Process '{name}' matched override '{override.get('name')}'"
                journal.send(msg)
                if debug:
                    print(msg)
                state["seen"].add(name)
            matched = True
        else:
            if name in state["seen"]:
                if debug:
                    journal.send(f"dynamic_power_user: Process '{name}' no longer running.")
                state["seen"].remove(name)

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
