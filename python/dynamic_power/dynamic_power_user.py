
import os
import time
import yaml
import psutil
import argparse
from systemd import journal

CONFIG_PATH = os.path.expanduser("~/.config/dynamic_power/config.yaml")
TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power-user.yaml"

class ConfigManager:
    def __init__(self):
        self.config = {}
        self.last_mtime = None
        self.load_config()

    def load_config(self):
        if os.path.exists(CONFIG_PATH):
            path = CONFIG_PATH
        else:
            path = TEMPLATE_PATH

        try:
            mtime = os.path.getmtime(path)
            if mtime != self.last_mtime:
                with open(path, 'r') as f:
                    self.config = yaml.safe_load(f) or {}
                self.last_mtime = mtime
        except Exception as e:
            journal.send(f"dynamic_power_user: Error loading config: {e}")

    def reload_if_needed(self):
        self.load_config()

    def get_thresholds(self):
        thresholds = self.config.get("load_thresholds", {})
        return thresholds.get("low", 1.0), thresholds.get("high", 2.0)

    def get_poll_interval(self):
        return self.config.get("general", {}).get("poll_interval", 5)

    def get_process_overrides(self):
        return self.config.get("process_overrides", [])

def send_thresholds_to_daemon(low, high, debug=False):
    try:
        import dbus
        bus = dbus.SystemBus()
        proxy = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
        iface = dbus.Interface(proxy, dbus_interface="org.dynamic_power.Daemon")
        result = iface.SetLoadThresholds(float(low), float(high))
        if debug:
            journal.send(f"dynamic_power_user: Sent thresholds to daemon: low={low}, high={high}, result={result}")
    except Exception as e:
        journal.send(f"dynamic_power_user: Failed to send thresholds to daemon: {e}")

def scan_processes(process_overrides, state, debug=False):
    seen_now = set()
    process_list = list(psutil.process_iter(['name', 'cmdline']))

    for proc in process_list:
        name = proc.info.get("name") or ""
        cmdline = " ".join(proc.info.get("cmdline") or [])

        for override in process_overrides:
            target = override.get("process_name")
            profile = override.get("active_profile")

            if not target:
                continue

            if target in name or target in cmdline:
                seen_now.add(target)
                if debug or target not in state["seen"]:
                    journal.send(f"dynamic_power_user: Matched override: {target} (profile={profile})")

    if debug:
        running_names = {proc.info['name'] for proc in process_list if proc.info.get("name")}
        journal.send(f"dynamic_power_user: Running processes: {', '.join(sorted(running_names))}")

    state["seen"] = seen_now

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
