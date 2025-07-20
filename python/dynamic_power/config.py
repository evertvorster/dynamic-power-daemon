import os
import shutil
import yaml
import pwd
import grp
import sys

from .power_profiles import normalize_profile
from .debug import debug_log, info_log, error_log

DEFAULT_CONFIG_PATH = "/etc/dynamic-power.yaml"
DEFAULT_TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power.yaml"

class Config:
    def __init__(self, path=DEFAULT_CONFIG_PATH):
        self.path = path
        self.data = {}
        self.last_loaded = 0
        self.load()

    def load(self):
        if not os.path.exists(self.path):
            info_log("config", "No config found. Generating from default.")
            self._generate_from_default()
        with open(self.path, "r") as f:
            self.data = yaml.safe_load(f) or {}
        self.last_loaded = os.path.getmtime(self.path)
        debug_log("config", f"Config loaded from {self.path}")
        self._maybe_upgrade()

    def reload_if_needed(self):
        try:
            mtime = os.path.getmtime(self.path)
            if mtime > self.last_loaded:
                self.load()
        except FileNotFoundError:
            pass

    def _generate_from_default(self):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        try:
            shutil.copy(DEFAULT_TEMPLATE_PATH, self.path)
            uid = os.geteuid()
            if uid == 0:
                os.chown(self.path, pwd.getpwnam("root").pw_uid, grp.getgrnam("root").gr_gid)
        except Exception as e:
            error_log("config", f"Failed to generate default config: {e}")
            raise
        info_log("config", f"Created new config at {self.path}")

    def _maybe_upgrade(self):
        try:
            with open(DEFAULT_TEMPLATE_PATH, "r") as f:
                template = yaml.safe_load(f)
            template_ver = template.get("version", 1)
            current_ver = self.data.get("version", 1)

            if template_ver > current_ver:
                backup_path = self.path + f".v{current_ver}.bak"
                shutil.copy(self.path, backup_path)
                info_log("config", f"Upgrading config: version {current_ver} -> {template_ver}")
                debug_log("config", f"Backed up old config to {backup_path}")
                shutil.copy(DEFAULT_TEMPLATE_PATH, self.path)
                os.chown(self.path, pwd.getpwnam("root").pw_uid, grp.getgrnam("root").gr_gid)
                self.load()
        except Exception as e:
            error_log("config", f"Failed to upgrade config: {e}")

    def _merge(self, key_path):
        def get_nested(d, keys):
            for k in keys:
                if not isinstance(d, dict):
                    return {}
                d = d.get(k, {})
            return d

        keys = key_path.split(".")
        return get_nested(self.data, keys)


    def get_profile(self, load_level, power_source):
        profiles = self._merge("power.profiles")
        if power_source == "battery":
            profile = profiles.get("on_battery", {}).get("default", "powersave")
        else:
            profile = profiles.get("on_ac", {}).get(load_level, "balanced")
        debug_log("config", f"get_profile(load_level={load_level}, power_source={power_source}) -> {profile}")
        return profile

    def get_thresholds(self):
        return self._merge("power.load_thresholds")

    def get_poll_interval(self):
        return self.data.get("general", {}).get("poll_interval")