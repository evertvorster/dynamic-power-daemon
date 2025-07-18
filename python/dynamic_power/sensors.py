import os

def find_power_devices():
    base = "/sys/class/power_supply"
    ac_device = None
    bat_device = None

    try:
        for name in os.listdir(base):
            type_path = os.path.join(base, name, "type")
            try:
                with open(type_path, "r") as f:
                    dev_type = f.read().strip()
                if dev_type == "Mains" and not ac_device:
                    ac_device = name
                elif dev_type == "Battery" and not bat_device:
                    bat_device = name
            except FileNotFoundError:
                continue
    except FileNotFoundError:
        pass

    return ac_device, bat_device

def get_power_source(ac_id=None, battery_id=None):
    fallback_used = False

    if not ac_id or not battery_id:
        ac_id, battery_id = find_power_devices()
        fallback_used = True

    ac_path = f"/sys/class/power_supply/{ac_id}/online" if ac_id else None
    bat_path = f"/sys/class/power_supply/{battery_id}/status" if battery_id else None

    try:
        if ac_path:
            with open(ac_path, "r") as f:
                ac_status = f.read().strip()
                if ac_status == "1":
                    if fallback_used:
                        print(f"[sensors] Detected AC online using fallback device: {ac_id}")
                    return "AC"
    except FileNotFoundError:
        pass

    try:
        if bat_path:
            with open(bat_path, "r") as f:
                status = f.read().strip().lower()
                if "discharging" in status:
                    if fallback_used:
                        print(f"[sensors] Detected battery discharging using fallback device: {battery_id}")
                    return "Battery"
                elif "charging" in status or "full" in status:
                    if fallback_used:
                        print(f"[sensors] Detected battery charging/full using fallback device: {battery_id}")
                    return "AC"
    except FileNotFoundError:
        pass

    print("[sensors] Warning: unable to determine power state")
    return "Unknown"

def get_load_average():
    try:
        loadavg = os.getloadavg()[0]
        return loadavg
    except OSError:
        print("[sensors] Error: could not read system load average")
        return 0.0

def classify_load(load, low_thresh=1.0, high_thresh=2.0):
    if load < low_thresh:
        return "low"
    elif load < high_thresh:
        return "medium"
    else:
        return "high"
