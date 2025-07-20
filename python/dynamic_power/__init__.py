import os
import sys
import time
import signal
from . import config, sensors, power_profiles, dbus_interface
from .debug import info_log, debug_log, error_log, DEBUG_ENABLED

terminate = False
override_profile = None  # Temporary profile override set via DBus
current_poll_interval = 1  # Default, overridden at runtime
current_threshold_low = 1.0
current_threshold_high = 2.0

def handle_term(signum, frame):
    global terminate
    info_log("main", f"Received signal {signum}, shutting down...")
    terminate = True

signal.signal(signal.SIGTERM, handle_term)
signal.signal(signal.SIGINT, handle_term)

def run():
    global override_profile, current_poll_interval, current_threshold_low, current_threshold_high

    info_log("main", "dynamic_power: starting daemon loop")

    cfg = config.Config()

    # Start DBus interface
    try:
        debug_log("main", "Attempting to start DBus interface...")
        dbus_interface.set_profile_override_callback(lambda p: set_override(p))
        dbus_interface.set_poll_interval_callback(lambda v: set_poll_interval(v))
        dbus_interface.set_thresholds_callback(lambda low, high: set_thresholds(low, high))
        dbus_interface.start_dbus_interface()
        debug_log("main", "DBus interface started successfully.")
    except Exception as e:
        error_log("main", f"Failed to start DBus interface: {e}")

    # Set initial profile
    power_profiles.set_profiles("performance")

    grace = cfg.data.get("general", {}).get("grace_period", 0)
    if grace > 0:
        debug_log("main", f"Grace period active: {grace} seconds in 'performance' mode")
        time.sleep(grace)

    current_poll_interval = cfg.get_poll_interval()
    thresholds = cfg.get_thresholds()
    current_threshold_low = thresholds.get("low", 1.0)
    current_threshold_high = thresholds.get("high", 2.0)

    while not terminate:
        cfg.reload_if_needed()

        if override_profile:
            debug_log("main", f"DBus override active: {override_profile}")
            power_profiles.set_profile(override_profile)
            time.sleep(current_poll_interval)
            continue

        power_source = sensors.get_power_source(
            cfg.data.get("power", {}).get("power_source", {})
        )
        debug_log("main", f"Detected power source: {power_source}")

        load_level = sensors.get_load_level(current_threshold_low, current_threshold_high)
        debug_log("main", f"Detected load level: {load_level}")

        profile = cfg.get_profile(load_level, power_source)

        power_profiles.set_profile(profile)

        time.sleep(current_poll_interval)

    info_log("main", "dynamic_power shut down cleanly.")

def set_override(profile):
    global override_profile
    override_profile = profile
    info_log("main", f"DBus override set: {profile}")

def set_poll_interval(interval):
    global current_poll_interval
    current_poll_interval = max(1, interval)
    info_log("main", f"Poll interval updated via DBus: {current_poll_interval} seconds")

def set_thresholds(low, high):
    global current_threshold_low, current_threshold_high
    current_threshold_low = max(0, float(low))
    current_threshold_high = max(current_threshold_low + 0.1, float(high))  # ensure spacing
    info_log("main", f"Thresholds updated via DBus: low={current_threshold_low}, high={current_threshold_high}")