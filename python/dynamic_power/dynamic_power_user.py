#!/usr/bin/env python3
import os
import time
import yaml
import psutil

from pathlib import Path

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
            print(f"[INFO] Config loaded from: {self.config_path}")
        except Exception as e:
            print(f"[ERROR] Failed to load config: {e}")
            self.data = {}

    def reload_if_needed(self):
        try:
            mtime = os.path.getmtime(self.config_path)
            if mtime > self.last_mtime:
                print("[INFO] Detected config change, reloading...")
                self.load()
        except FileNotFoundError:
            pass

    def get_process_overrides(self):
        return self.data.get("process_overrides", [])

def check_process_matches(overrides):
    matched = []

    for proc in psutil.process_iter(attrs=["name"]):
        pname = proc.info.get("name")
        for override in overrides:
            if pname == override.get("process_name"):
                print(f"[MATCH] Detected matching process: {pname}")
                matched.append(override)

    if matched:
        matched.sort(key=lambda o: o.get("priority", 0), reverse=True)
        return matched[0]

    return None

def main():
    print("[INFO] Starting dynamic_power_user monitor")
    config = ConfigManager()

    try:
        while True:
            config.reload_if_needed()

            override = check_process_matches(config.get_process_overrides())
            if override:
                print(f"[ACTIVE] Override matched: {override.get('name')} -> {override}")

            time.sleep(CONFIG_CHECK_INTERVAL)

    except KeyboardInterrupt:
        print("\n[INFO] Exiting dynamic_power_user monitor")

if __name__ == "__main__":
    main()