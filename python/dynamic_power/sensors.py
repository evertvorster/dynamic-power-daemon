import os
import subprocess
import re
import logging
import json
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
    """Returns a dictionary of connected displays with current, min, max refresh rates, and resolution."""
    if cfg and not cfg.get("features", {}).get("screen_refresh", False):
        return None

    try:
        output = subprocess.check_output(["kscreen-doctor", "-j"], text=True)
        data = json.loads(output)
    except (subprocess.CalledProcessError, FileNotFoundError, json.JSONDecodeError) as e:
        logging.info(f"sensors: Failed to query kscreen-doctor -j: {e}")
        return None

    displays = {}

    for output in data.get("outputs", []):
        name = output.get("name")
        current_mode_id = output.get("currentModeId")
        modes = output.get("modes", [])
        if not name or not current_mode_id or not modes:
            continue

        refresh_rates = []
        resolution = None
        current_refresh = None

        for mode in modes:
            if mode.get("id") == current_mode_id:
                current_refresh = int(round(mode.get("refreshRate", 0)))
                resolution = f"{mode['size']['width']}x{mode['size']['height']}"
            if "refreshRate" in mode:
                refresh_rates.append(int(round(mode["refreshRate"])))

        if not current_refresh or not resolution or not refresh_rates:
            continue

        displays[name] = {
            "current": current_refresh,
            "resolution": resolution,
            "min": min(refresh_rates),
            "max": max(refresh_rates)
        }

        logging.debug(f"sensors: Detected {name}: {resolution} @ {current_refresh}Hz (min={min(refresh_rates)}, max={max(refresh_rates)})")

    return displays if displays else None


def set_refresh_rates_for_power(power_src: str):
    """
    Sets each screen to its min (on battery) or max (on AC) refresh rate
    at the current resolution, using kscreen-doctor.
    """
    from .sensors import get_refresh_info  # avoid circular import if used elsewhere

    refresh_data = get_refresh_info()
    if not refresh_data:
        logging.info("[sensors] No refresh data available, skipping refresh adjustment")
        return

    for output, info in refresh_data.items():
        resolution = info.get("resolution")
        if not resolution:
            logging.info(f"[sensors] No resolution info for {output}, skipping")
            continue

        target_rate = info.get("min") if power_src == "BAT" else info.get("max")
        if not target_rate:
            logging.info(f"[sensors] No target refresh rate for {output}, skipping")
            continue

        mode_string = f"{resolution}@{target_rate}"
        cmd = ["kscreen-doctor", f"output.{output}.mode.{mode_string}"]

        try:
            subprocess.run(cmd, check=True)
            logging.info(f"[sensors] Set {output} to {mode_string}")
        except subprocess.CalledProcessError as e:
            logging.info(f"[sensors] Failed to set {output} to {mode_string}: {e}")


def get_panel_overdrive_status() -> bool | None:
    try:
        import dbus
        bus = dbus.SystemBus()
        obj = bus.get_object("xyz.ljones.Asusd", "/xyz/ljones/asus_armoury/panel_overdrive")
        props = dbus.Interface(obj, "org.freedesktop.DBus.Properties")
        value = props.Get("xyz.ljones.AsusArmoury", "CurrentValue")
        return bool(value)
    except dbus.DBusException as e:
        logging.info(f"sensors: DBus error querying panel_overdrive: {e}")
    except Exception as e:
        logging.info(f"sensors: Failed to get panel_overdrive via DBus: {e}")
    return None
