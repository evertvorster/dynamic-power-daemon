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

    # ───────────────────────────────────────── internal helpers ───
    def _generate_from_default(self):
        try:
            shutil.copy(DEFAULT_TEMPLATE_PATH, self.path)
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

    # NEW ⟶ over‑drive toggle rules
    def get_panel_overdrive_config(self):
        """
        Returns a dict like {"enable_on_ac": True}.  If the key is missing
        in /etc/dynamic-power.yaml we default to True so the feature is
        active out‑of‑the‑box.
        """
        cfg = self._merge("panel.overdrive")
        if not isinstance(cfg, dict):
            cfg = {}
        return {"enable_on_ac": cfg.get("enable_on_ac", True)}


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
