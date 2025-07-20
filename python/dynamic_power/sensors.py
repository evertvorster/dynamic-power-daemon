import os
from .debug import debug_log

def get_power_source(power_source_cfg=None):
    ac_id = "ADP0"
    battery_id = "BAT0"

    if isinstance(power_source_cfg, dict):
        ac_id = power_source_cfg.get("ac_id", ac_id)
        battery_id = power_source_cfg.get("battery_id", battery_id)

    ac_path = f"/sys/class/power_supply/{ac_id}/online"
    try:
        with open(ac_path, "r") as f:
            online = f.read().strip() == "1"
            debug_log("sensors", f"Detected power source: {'AC' if online else 'Battery'}")
            return "ac" if online else "battery"
    except FileNotFoundError:
        debug_log("sensors", f"Fallback detection failed, using default AC device: {ac_id}")
        return "ac"

def get_load_level(low_th=1.0, high_th=2.0):
    try:
        with open("/proc/loadavg", "r") as f:
            load_avg = float(f.read().split()[0])
    except Exception as e:
        debug_log("sensors", f"Failed to read loadavg: {e}")
        return "low"

    level = "low"
    if load_avg > high_th:
        level = "high"
    elif load_avg > low_th:
        level = "medium"

    debug_log("sensors", f"Load average: {load_avg}, Level: {level}")
    return level