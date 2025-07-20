import os
import sys
import time
import signal
from . import config, sensors, power_profiles, utils, dbus_interface
from .debug import info_log, debug_log, error_log, DEBUG_ENABLED

terminate = False
override_profile = None  # DBus-set temporary profile override

def handle_term(signum, frame):
    global terminate
    info_log("main", f"Received signal {signum}, shutting down...")
    terminate = True

signal.signal(signal.SIGTERM, handle_term)
signal.signal(signal.SIGINT, handle_term)

def set_override(profile):
    global override_profile
    override_profile = profile
    info_log("main", f"DBus override set: {profile}")

def run():
    info_log("main", "dynamic_power: starting daemon loop")

    # Start DBus interface
    try:
        debug_log("main", "Attempting to start DBus interface...")
        dbus_interface.set_profile_override_callback(set_override)
        dbus_interface.start_dbus_interface()
        debug_log("main", "DBus interface started successfully.")
    except Exception as e:
        error_log("main", f"Failed to start DBus interface: {e}")

    cfg = config.Config()

    # Set initial profile
    power_profiles.set_profiles("performance")

    grace = cfg.data.get("general", {}).get("grace_period", 0)
    if grace > 0:
        debug_log("main", f"Grace period active: {grace} seconds in 'performance' mode")
        time.sleep(grace)

    poll_interval = cfg.data.get("general", {}).get("poll_interval", 1)

    while not terminate:
        cfg.reload_if_needed()

        # 1. Check for DBus override
        if override_profile:
            debug_log("main", f"DBus override active: {override_profile}")
            power_profiles.set_profile(override_profile)
            time.sleep(poll_interval)
            continue

        # 2. Check for process overrides
        override = utils.get_process_override(cfg.data)
        if override:
            debug_log("main", f"Active process override: {override}")
            profile = override.get("active_profile")
            if profile:
                power_profiles.set_profile(profile)

            time.sleep(poll_interval)
            continue

        # 3. Normal dynamic behaviour
        power_source = sensors.get_power_source(
            cfg.data.get("power", {}).get("power_source", {})
        )
        debug_log("main", f"Detected power source: {power_source}")

        load_level = sensors.get_load_level(cfg.data)
        debug_log("main", f"Detected load level: {load_level}")

        profile = cfg.get_profile(load_level, power_source)

        power_profiles.set_profile(profile)

        time.sleep(poll_interval)

    info_log("main", "dynamic_power shut down cleanly.")
