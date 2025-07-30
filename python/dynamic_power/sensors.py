import os
import subprocess
import re
import logging
from .config import is_debug_enabled

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
            logging.debug("sensors: Detected power source: AC" if online else "sensors: Detected power source: Battery")
            return "ac" if online else "battery"
    except FileNotFoundError:
        logging.info(f"sensors: Fallback detection failed, using default AC device: {ac_id}")
        return "ac"

def get_load_level(low_th=1.0, high_th=2.0):
    try:
        with open("/proc/loadavg", "r") as f:
            load_avg = float(f.read().split()[0])
    except Exception as e:
        logging.info(f"sensors: Failed to read loadavg: {e}")
        return "low"

    level = "low"
    if load_avg > high_th:
        level = "high"
    elif load_avg > low_th:
        level = "medium"

    logging.debug(f"sensors: Load average: {load_avg}, Level: {level}")
    return level

def get_uptime_seconds():
    with open("/proc/uptime", "r") as f:
        return float(f.readline().split()[0])

def get_process_override(config):
    overrides = config.get("process_overrides", {})
    if not overrides:
        return None

    max_priority = -1
    selected_profile = None
    for proc in os.listdir("/proc"):
        if not proc.isdigit():
            continue
        try:
            with open(f"/proc/{proc}/comm", "r") as f:
                name = f.read().strip()
            if name in overrides:
                entry = overrides[name]
                priority = entry.get("priority", 0)
                if priority > max_priority:
                    max_priority = priority
                    selected_profile = entry.get("power_mode")
        except FileNotFoundError:
            continue
    return selected_profile

def get_refresh_info(cfg=None):
    """Returns a dictionary of connected displays with current, min, and max refresh rates."""
    if cfg and not cfg.get("features", {}).get("screen_refresh", False):
        return None

    try:
        output = subprocess.check_output(["kscreen-doctor", "-o"], text=True)
        # strip ANSI colour codes produced by kscreen-doctor
        output = re.sub(r"\x1b\[[0-9;]*m", "", output)

    except (subprocess.CalledProcessError, FileNotFoundError):
        logging.info("sensors: kscreen-doctor not found or failed.")
        return None

    displays = {}
    current_display = None
    all_modes = {}

    for raw in output.splitlines():
        line = raw.strip()

        # new display section
        if line.startswith("Output:"):
            parts = line.split()
            if len(parts) >= 3:
                current_display = parts[2]
                all_modes[current_display] = []
            continue

        if current_display is None:
            continue  # ignore lines until we know which display we’re in

        # collect every numeric refresh rate on this line
        for hz in re.findall(r"@(\d+)", line):
            all_modes[current_display].append(int(hz))

        # detect current mode marked with * or *!
        star = re.search(r"@(?P<hz>\d+)\*(?:!?)", line)
        if star:
            displays[current_display] = {"current": int(star.group("hz"))}

    for name, rates in all_modes.items():
        if name in displays and rates:
            displays[name]["min"] = min(rates)
            displays[name]["max"] = max(rates)

    logging.info(f"sensors: Detected refresh info: {displays}")
    return displays if displays else None

def get_panel_overdrive_status() -> bool | None:
    try:
        result = subprocess.run(
            ["asusctl", "armoury", "--help"],
            capture_output=True, text=True, check=True
        )
        lines = result.stdout.splitlines()
        inside_panel_block = False
        for line in lines:
            if "panel_overdrive:" in line:
                inside_panel_block = True
            elif inside_panel_block and "current:" in line:
                if "[0,(1)]" in line:
                    return True
                elif "[(0),1]" in line:
                    return False
                break
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        logging.info(f"sensors: Failed to query panel overdrive status: {e}")
    return None
