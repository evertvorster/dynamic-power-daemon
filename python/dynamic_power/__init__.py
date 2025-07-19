
import os
import sys
import time
from . import config, sensors, power_profiles, epp, utils, debug

DEBUG_ENABLED = debug.DEBUG_ENABLED

def run():
    print("dynamic_power: starting daemon loop")

    cfg = config.Config()

    # Detect CPU profiles & just log for now
    power_profiles.log_available_governors()
    
    # Detect EPP modes before setting any profile
    epp.detect_supported_modes()

    # Now set a known initial profile (used for grace period later)
    power_profiles.set_profiles("performance")

    poll_interval = cfg.data.get("general", {}).get("poll_interval", 1)

    while True:
        cfg.reload_if_needed()

        override = utils.get_process_override(cfg.data)
        if override:
            if DEBUG_ENABLED:
                print(f"[debug:main] Active override: {override}")
            profile = override.get("active_profile")
            epp_value = override.get("active_epp")

            if profile:
                power_profiles.set_profile(profile)
            if epp_value:
                epp.set_epp(epp_value)

            time.sleep(poll_interval)
            continue

        power_source = sensors.get_power_source(
            cfg.data.get("power", {}).get("power_source", {})
        )
        if DEBUG_ENABLED:
            print(f"[debug] Detected power source: {power_source}")

        load_level = sensors.get_load_level(cfg.data)
        if DEBUG_ENABLED:
            print(f"[debug] Detected load level: {load_level}")

        profile = cfg.get_profile(load_level, power_source)
        epp_value = cfg.get_epp(profile)

        power_profiles.set_profile(profile)
        epp.set_epp(epp_value)

        time.sleep(poll_interval)
