
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
            self.data = yaml.safe_load(f)
        self.last_loaded = os.path.getmtime(self.path)
        debug_log("config", f"Config loaded from {self.path}")

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

    def get_profile(self, load_level, power_source):
        profiles = self.data.get("power", {}).get("profiles", {})
        if power_source == "battery":
            profile = profiles.get("on_battery", {}).get("default", "powersave")
        else:
            profile = profiles.get("on_ac", {}).get(load_level, "balanced")
        debug_log("config", f"get_profile(load_level={load_level}, power_source={power_source}) -> {profile}")
        return profile

    def get_epp(self, profile_name):
        if not self.data.get("epp", {}).get("enabled", False):
            debug_log("config", "EPP disabled in config")
            return None
        normalized = normalize_profile(profile_name)
        epp_value = self.data.get("epp", {}).get("values", {}).get(normalized)
        debug_log("config", f"get_epp({profile_name}) normalized -> {normalized}, value -> {epp_value}")
        return epp_value
