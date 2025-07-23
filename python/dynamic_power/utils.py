# dynamic_power/utils.py

import os
import time
import yaml

def get_uptime_seconds():
    with open("/proc/uptime", "r") as f:
        return float(f.readline().split()[0])

def write_metrics_file(data):
    uid = os.getuid()
    path = f"/run/user/{uid}/dynamic_power_metrics.yaml"
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        yaml.dump(data, f)

def read_metrics_file():
    uid = os.getuid()
    path = f"/run/user/{uid}/dynamic_power_metrics.yaml"
    try:
        with open(path, "r") as f:
            return yaml.safe_load(f)
    except FileNotFoundError:
        return {}

__all__ = [
    "get_uptime_seconds",
    "write_metrics_file",
    "read_metrics_file",
]
