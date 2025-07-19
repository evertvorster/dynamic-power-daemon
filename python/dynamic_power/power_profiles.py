import subprocess
from .debug import DEBUG_ENABLED

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

def get_current_profile():
    try:
        output = subprocess.check_output(["powerprofilesctl", "get"], text=True).strip()
        return output
    except Exception as e:
        if DEBUG_ENABLED:
            print(f"[power_profiles] Failed to get current profile: {e}")
        return None

def set_profile(profile_name):
    actual_profile = normalize_profile(profile_name)
    current = get_current_profile()
    if current == actual_profile:
        return  # Already active
    try:
        subprocess.check_call(["powerprofilesctl", "set", actual_profile])
        print(f"[power_profiles] Switched to profile: {actual_profile}")
    except Exception as e:
        print(f"[power_profiles] Failed to set profile '{actual_profile}': {e}")

def set_profiles(profile_name):
    # Show available CPU profiles (e.g., platform profiles) if present
    available_profiles_path = "/sys/firmware/acpi/platform_profile/available_profiles"
    try:
        with open(available_profiles_path, "r") as f:
            profiles = f.read().strip()
            if DEBUG_ENABLED:
                print(f"[power_profiles] Available CPU profiles: {profiles}")
    except FileNotFoundError:
        if DEBUG_ENABLED:
            print("[power_profiles] Could not read available CPU profiles (file missing)")

    set_profile(profile_name)
