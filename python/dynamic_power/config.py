
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
USER_CONFIG_PATH = os.path.expanduser("~/.config/dynamic_power/config.yaml")

class Config:
    def __init__(self, path=DEFAULT_CONFIG_PATH):
        self.path = path
        self.data = {}
        self.user_data = {}
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
        self._load_user_config()

    def _load_user_config(self):
        if os.path.exists(USER_CONFIG_PATH):
            try:
                with open(USER_CONFIG_PATH, "r") as f:
                    self.user_data = yaml.safe_load(f) or {}
                debug_log("config", f"User config loaded from {USER_CONFIG_PATH}")
            except Exception as e:
                error_log("config", f"Failed to load user config: {e}")
        else:
            debug_log("config", "No user config found")

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

    def _merge(self, key_path):
        def get_nested(d, keys):
            for k in keys:
                if not isinstance(d, dict):
                    return {}
                d = d.get(k, {})
            return d

        keys = key_path.split(".")
        system_value = get_nested(self.data, keys)
        user_value = get_nested(self.user_data, keys)
        merged = {**system_value, **user_value}
        return merged

    def get_profile(self, load_level, power_source):
        profiles = self._merge("power.profiles")
        if power_source == "battery":
            profile = profiles.get("on_battery", {}).get("default", "powersave")
        else:
            profile = profiles.get("on_ac", {}).get(load_level, "balanced")
        debug_log("config", f"get_profile(load_level={load_level}, power_source={power_source}) -> {profile}")
        return profile

    def get_epp(self, profile_name):
        epp_config = self._merge("epp.values")
        enabled = self.user_data.get("epp", {}).get("enabled", None)
        if enabled is None:
            enabled = self.data.get("epp", {}).get("enabled", False)
        if not enabled:
            debug_log("config", "EPP disabled in config")
            return None
        normalized = normalize_profile(profile_name)
        epp_value = epp_config.get(normalized)
        debug_log("config", f"get_epp({profile_name}) normalized -> {normalized}, value -> {epp_value}")
        return epp_value

    def get_thresholds(self):
        return self._merge("power.load_thresholds")

    def get_poll_interval(self):
        sys_val = self.data.get("general", {}).get("poll_interval")
        user_val = self.user_data.get("general", {}).get("poll_interval")
        return user_val if user_val is not None else sys_val
