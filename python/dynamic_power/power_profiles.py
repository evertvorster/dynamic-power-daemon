
import time
import subprocess
from .debug import DEBUG_ENABLED, debug_log, info_log, error_log

PROFILE_ALIASES = {
    "powersave":    "power-saver",
    "balanced":     "balanced",
    "performance":  "performance",
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

def set_profile(profile_name, retries=2, delay=0.2):
    actual_profile = normalize_profile(profile_name)
    current = get_current_profile()
    if current == actual_profile:
        debug_log("power_profiles", f"Profile '{actual_profile}' already active – skipping")
        return

    for attempt in range(1, retries + 2):  # 1 initial try + N retries
        try:
            subprocess.check_call(["powerprofilesctl", "set", actual_profile])
            time.sleep(delay)
            confirmed = get_current_profile()
            if confirmed == actual_profile:
                debug_log("power_profiles", f"Switched to profile: {actual_profile} (confirmed on attempt {attempt})")
                return
            else:
                debug_log("power_profiles", f"Attempt {attempt}: profile still '{confirmed}', expected '{actual_profile}'")
        except Exception as e:
            error_log("power_profiles", f"Attempt {attempt} failed to set profile '{actual_profile}': {e}")

    error_log("power_profiles", f"Failed to confirm profile '{actual_profile}' after {retries + 1} attempts")

def set_profiles(profile_name):
    set_profile(profile_name)
