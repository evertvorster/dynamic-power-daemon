import os
import shutil
import yaml
import pwd
import grp
from .power_profiles import normalize_profile

DEFAULT_CONFIG_PATH = "/etc/dynamic-power.yaml"
DEFAULT_TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power.yaml"

class Config:
    def __init__(self, path=DEFAULT_CONFIG_PATH, debug=False):
        self.path = path
        self.data = {}
        self.last_loaded = 0
        self.debug = debug
        self.load()

    def debug_print(self, msg):
        if self.debug:
            print(f"[debug:config] {msg}")

    def load(self):
        if not os.path.exists(self.path):
            print("[dynamic_power] No config found. Generating from default.")
            self._generate_from_default()
        with open(self.path, "r") as f:
            self.data = yaml.safe_load(f)
        self.last_loaded = os.path.getmtime(self.path)
        self.debug_print(f"Config loaded from {self.path}")

    def reload_if_needed(self):
        try:
            mtime = os.path.getmtime(self.path)
            if mtime > self.last_loaded:
                self.load()
                self.debug_print("Config reloaded due to file modification.")
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
            print(f"[dynamic_power] Failed to generate default config: {e}")
            raise

        print(f"[dynamic_power] Created new config at {self.path}")

    def get_profile(self, load_level, power_source):
        profiles = self.data.get("power", {}).get("profiles", {})
        if power_source == "battery":
            return profiles.get("on_battery", {}).get("default", "powersave")
        return profiles.get("on_ac", {}).get(load_level, "balanced")

    def get_epp(self, profile_name):
        if not self.data.get("epp", {}).get("enabled", False):
            return None
        profile_name = normalize_profile(profile_name)
        return self.data.get("epp", {}).get("values", {}).get(profile_name)
