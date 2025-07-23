import os
import subprocess
import time
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

def get_power_source(ac_id: str) -> str:
    """
    Return 'AC' if AC power is online, else 'Battery'.
    """
    try:
        path = f"/sys/class/power_supply/{ac_id}/online"
        with open(path, "r") as f:
            return "AC" if f.read().strip() == "1" else "Battery"
    except Exception as e:
        print(f"[sensors.py] Failed to read power source: {e}")
        return "Unknown"

def get_battery_status(battery_id: str) -> str:
    """
    Return battery status string from /sys/class/power_supply/{battery_id}/status
    """
    try:
        path = f"/sys/class/power_supply/{battery_id}/status"
        with open(path, "r") as f:
            return f.read().strip()
    except Exception as e:
        print(f"[sensors.py] Failed to read battery status: {e}")
        return "Unknown"

def get_cpu_load(interval=0.1) -> float:
    """
    Read CPU load average over the specified interval.
    """
    with open("/proc/stat", "r") as f:
        fields = f.readline().strip().split()[1:]
        prev_idle = int(fields[3])
        prev_total = sum(map(int, fields))

    time.sleep(interval)

    with open("/proc/stat", "r") as f:
        fields = f.readline().strip().split()[1:]
        idle = int(fields[3])
        total = sum(map(int, fields))

    idle_delta = idle - prev_idle
    total_delta = total - prev_total

    return 1.0 - idle_delta / total_delta if total_delta != 0 else 0.0

def get_cpu_freq() -> tuple[int, int]:
    """
    Return a tuple of (current_freq_khz, max_freq_khz) for CPU0.
    """
    try:
        with open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r") as f:
            cur = int(f.read().strip())
        with open("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r") as f:
            max_ = int(f.read().strip())
        return cur, max_
    except Exception as e:
        print(f"[sensors.py] Failed to read CPU freq: {e}")
        return (0, 0)

def set_panel_autohide(enabled: bool) -> None:
    """
    Sets the KDE panel autohide state.
    """
    mode = "1" if enabled else "0"
    try:
        subprocess.run([
            "qdbus",
            "org.kde.plasmashell",
            "/PlasmaShell",
            "org.kde.PlasmaShell.evaluateScript",
            f"""
            var panels = desktops()[0].panels();
            for (var i = 0; i < panels.length; ++i) {{
                panels[i].hiding = {mode};
            }}
            """
        ], check=True)
    except Exception as e:
        print(f"[sensors.py] Failed to set panel autohide: {e}")

def set_refresh_rate(enabled: bool) -> None:
    """
    Stub for refresh rate control. Not yet implemented.
    """
    print(f"[sensors.py] set_refresh_rate({enabled}) called (stub)")
