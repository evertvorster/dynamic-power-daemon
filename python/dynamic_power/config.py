import os
import shutil
import yaml
import pwd
import grp
import sys
import logging

from .power_profiles import normalize_profile
from .debug import debug_log, info_log, error_log

# Debug flag from environment.
DEBUG = os.getenv("DYNAMIC_POWER_DEBUG") == "1"
logging.basicConfig(
    level=logging.DEBUG if DEBUG else logging.INFO,
    format="%(levelname)s: %(message)s"
)
def is_debug_enabled():
    return DEBUG

DEFAULT_CONFIG_PATH = "/etc/dynamic-power.yaml"
DEFAULT_TEMPLATE_PATH_ROOT = "/usr/share/dynamic-power/dynamic-power.yaml"
DEFAULT_TEMPLATE_PATH_USER = "/usr/share/dynamic-power/dynamic-power-user.yaml"

class Config:
    def __init__(self, path=DEFAULT_CONFIG_PATH):
        self.path = path
        self.data = {}
        self.last_loaded = 0
        self.load()

    # ───────────────────────────────────────── internal helpers ───
    def _generate_from_default(self):
        try:
            os.makedirs(os.path.dirname(self.path), exist_ok=True)
            template = DEFAULT_TEMPLATE_PATH_ROOT if os.geteuid() == 0 else DEFAULT_TEMPLATE_PATH_USER
            shutil.copy(template, self.path)
            if self.path.startswith("/etc/"):
                uid = pwd.getpwnam("root").pw_uid
                gid = grp.getgrnam("root").gr_gid
                os.chown(self.path, uid, gid)
            info_log("config", f"Default config copied to {self.path}")
        except Exception as e:
            error_log("config", f"Failed to copy default config: {e}")
            sys.exit(1)


    def _maybe_upgrade(self):
        # future‑proof placeholder
        pass

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

    def _merge(self, key_path):
        def get_nested(d, keys):
            for k in keys:
                if not isinstance(d, dict):
                    return {}
                d = d.get(k, {})
            return d
        return get_nested(self.data, key_path.split("."))

    # ───────────────────────────────────────── public accessors ───
    def get_profile(self, load_level, power_source):
        profiles = self._merge("power.profiles")
        if power_source == "battery":
            profile = profiles.get("on_battery", {}).get("default", "powersave")
        else:
            profile = profiles.get("on_ac", {}).get(load_level, "balanced")
        debug_log("config",
                  f"get_profile(load_level={load_level}, power_source={power_source}) -> {profile}")
        return profile

    def get_thresholds(self):
        return self._merge("power.load_thresholds")

    def get_poll_interval(self):
        return self.data.get("general", {}).get("poll_interval")

    def get_panel_overdrive_config(self):
        """
        Returns a dict like {"enabled": True}, based on features.auto_panel_overdrive
        """
        enabled = self.data.get("features", {}).get("auto_panel_overdrive", False)
        return {"enabled": enabled}

    def get_screen_refresh_config(self):
        """Return the config block for screen_refresh inside features."""
        features = self.data.get("features", {})
        value = features.get("screen_refresh", False)
        return {"enabled": bool(value)}




# === Added for GUI support of user config ===
import yaml
from pathlib import Path

USER_CONFIG_PATH = Path.home() / ".config" / "dynamic_power" / "config.yaml"

def load_user_config():
    try:
        with open(USER_CONFIG_PATH, "r") as f:
            return yaml.safe_load(f) or {}
    except FileNotFoundError:
        return {}

def save_user_config(config):
    USER_CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    with open(USER_CONFIG_PATH, "w") as f:
        yaml.safe_dump(config, f)
        
def load_config():
    print("[DEBUG] USER_CONFIG_PATH =", USER_CONFIG_PATH)    
    return Config(str(USER_CONFIG_PATH))
 
