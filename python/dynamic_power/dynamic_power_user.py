#!/usr/bin/env python3
import os
import time
import yaml
import psutil
import dbus
from pathlib import Path
from systemd import journal

USER_CONFIG_PATH = Path.home() / ".config/dynamic_power/config.yaml"
SYSTEM_TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power.yaml"

CONFIG_CHECK_INTERVAL = 5  # seconds

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

def send_thresholds_to_daemon(low, high):
    try:
        bus = dbus.SystemBus()
        obj = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
        iface = dbus.Interface(obj, "org.dynamic_power.Daemon")
        result = iface.SetLoadThresholds(dbus.Double(low), dbus.Double(high))
        journal.send(f"dynamic_power_user: Sent thresholds to daemon: low={low}, high={high}, result={result}")
    except Exception as e:
        journal.send(f"dynamic_power_user: Failed to send thresholds: {e}")

def main():
    journal.send("dynamic_power_user: Starting monitor")
    config = ConfigManager()

    try:
        while True:
            config.reload_if_needed()

            low, high = config.get_thresholds()
            send_thresholds_to_daemon(low, high)

            time.sleep(CONFIG_CHECK_INTERVAL)

    except KeyboardInterrupt:
        journal.send("dynamic_power_user: Exiting monitor")

if __name__ == "__main__":
    main()