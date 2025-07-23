
import os
import time
import yaml

METRICS_PATH = f"/run/user/{os.getuid()}/dynamic_power_metrics.yaml"

def get_uptime_seconds():
    """Returns system uptime in seconds."""
    with open("/proc/uptime", "r") as f:
        uptime_str = f.readline().split()[0]
        return float(uptime_str)

def write_metrics_file(metrics):
    try:
        os.makedirs(os.path.dirname(METRICS_PATH), exist_ok=True)
        with open(METRICS_PATH, "w") as f:
            yaml.safe_dump(metrics, f)
    except Exception as e:
        print(f"[utils] Failed to write metrics file: {e}")

def read_metrics_file():
    try:
        with open(METRICS_PATH, "r") as f:
            return yaml.safe_load(f) or {}
    except FileNotFoundError:
        return {}
    except Exception as e:
        print(f"[utils] Failed to read metrics file: {e}")
        return {}

def get_power_source(ac_id="ADP0", battery_id="BAT0"):
    """Determine if the system is on AC or battery power."""
    try:
        with open(f"/sys/class/power_supply/{ac_id}/online", "r") as f:
            online = f.read().strip()
            return "ac" if online == "1" else "battery"
    except FileNotFoundError:
        return "unknown"

__all__ = [
    "get_uptime_seconds",
    "write_metrics_file",
    "read_metrics_file",
    "get_power_source",
]
