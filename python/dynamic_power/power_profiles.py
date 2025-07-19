
import subprocess
from .debug import DEBUG_ENABLED, debug_log, info_log, error_log

PROFILE_ALIASES = {
    "powersave": "power-saver",
    "power_save": "power-saver",
    "power-saver": "power-saver",
    "balanced": "balanced",
    "performance": "performance",
    "quiet": "power-saver",  # quiet mode still uses power-saver profile
}

def normalize_profile(profile):
    return PROFILE_ALIASES.get(profile, profile)

def log_available_governors():
    path = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors"
    try:
        with open(path, "r") as f:
            governors = f.read().strip()
            debug_log("power_profiles", f"Available CPU governors: {governors}")
    except FileNotFoundError:
        debug_log("power_profiles", "Could not read scaling_available_governors")

def get_current_profile():
    try:
        output = subprocess.check_output(["powerprofilesctl", "get"], text=True).strip()
        return output
    except Exception as e:
        error_log("power_profiles", f"Failed to get current profile: {e}")
        return None

def set_profile(profile_name):
    actual_profile = normalize_profile(profile_name)
    current = get_current_profile()
    if current == actual_profile:
        return  # Already active
    try:
        subprocess.check_call(["powerprofilesctl", "set", actual_profile])
        info_log("power_profiles", f"Switched to profile: {actual_profile}")
    except Exception as e:
        error_log("power_profiles", f"Failed to set profile '{actual_profile}': {e}")

def set_profiles(profile_name):
    set_profile(profile_name)
