import os

def get_power_source(ac_id=None, battery_id=None, debug=False):
    base = "/sys/class/power_supply"
    ac_path = os.path.join(base, ac_id or "AC")
    battery_path = os.path.join(base, battery_id or "BAT0")

    try:
        with open(os.path.join(ac_path, "online"), "r") as f:
            status = f.read().strip()
            if status == "1":
                if debug:
                    print(f"[debug:sensors] AC is online using {ac_path}")
                return "ac"
    except FileNotFoundError:
        if debug:
            print(f"[debug:sensors] AC device {ac_path} not found")

    try:
        with open(os.path.join(battery_path, "status"), "r") as f:
            status = f.read().strip().lower()
            if status == "discharging":
                if debug:
                    print(f"[debug:sensors] Battery {battery_path} is discharging")
                return "battery"
    except FileNotFoundError:
        if debug:
            print(f"[debug:sensors] Battery device {battery_path} not found")

    if debug:
        print(f"[sensors] Detected AC online using fallback device: {ac_path}")
    return "ac"

def get_load_level(config, debug=False):
    loadavg_path = "/proc/loadavg"
    try:
        with open(loadavg_path, "r") as f:
            load_str = f.read().split()[0]
            load = float(load_str)
            if debug:
                print(f"[debug:sensors] System load average: {load}")
    except Exception as e:
        if debug:
            print(f"[debug:sensors] Failed to read load average: {e}")
        return "low"

    thresholds = config.get("power", {}).get("load_thresholds", {})
    medium = float(thresholds.get("medium", 1))
    high = float(thresholds.get("high", 2))

    if load < medium:
        return "low"
    elif load < high:
        return "medium"
    else:
        return "high"
