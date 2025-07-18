
import subprocess

PROFILE_ALIASES = {
    "powersave": "power-saver",
    "power_save": "power-saver",
    "power-saver": "power-saver",
    "balanced": "balanced",
    "performance": "performance",
    "quiet": "quiet"
}

def normalize_profile(name):
    return PROFILE_ALIASES.get(name.lower(), name)

def get_active_profile():
    try:
        result = subprocess.run(["powerprofilesctl", "get"], capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"[power_profiles] Error getting current profile: {e}")
        return None

def set_profile(profile):
    profile = normalize_profile(profile)
    current = get_active_profile()
    if current == profile:
        return  # No need to change
    try:
        subprocess.run(["powerprofilesctl", "set", profile], check=True)
        print(f"[power_profiles] Switched to profile: {profile}")
    except subprocess.CalledProcessError as e:
        print(f"[power_profiles] Failed to set profile '{profile}': {e}")

def fix_scaling_max_freqs():
    try:
        subprocess.run(["/usr/lib/dynamic_power/fix_max_freq.sh"], check=True)
        print("[power_profiles] Scaling max frequencies fixed.")
    except subprocess.CalledProcessError as e:
        print(f"[power_profiles] Failed to fix scaling max freqs: {e}")
