
import os
import sys
import time
from . import config, sensors, power_profiles, utils, debug

DEBUG_ENABLED = debug.DEBUG_ENABLED
info_log = debug.info_log
debug_log = debug.debug_log

def run():
    info_log("main", "dynamic_power: starting daemon loop")

    cfg = config.Config()

    # Set initial profile
    power_profiles.set_profiles("performance")

    grace = cfg.data.get("general", {}).get("grace_period", 0)
    if grace > 0:
        debug_log("main", f"Grace period active: {grace} seconds in 'performance' mode")
        time.sleep(grace)

    poll_interval = cfg.data.get("general", {}).get("poll_interval", 1)

    while True:
        cfg.reload_if_needed()

        override = utils.get_process_override(cfg.data)
        if override:
            debug_log("main", f"Active override: {override}")
            profile = override.get("active_profile")
            if profile:
                power_profiles.set_profile(profile)

            time.sleep(poll_interval)
            continue

        power_source = sensors.get_power_source(
            cfg.data.get("power", {}).get("power_source", {})
        )
        debug_log("main", f"Detected power source: {power_source}")

        load_level = sensors.get_load_level(cfg.data)
        debug_log("main", f"Detected load level: {load_level}")

        profile = cfg.get_profile(load_level, power_source)

        power_profiles.set_profile(profile)

        time.sleep(poll_interval)
