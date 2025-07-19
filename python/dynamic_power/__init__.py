import os
import sys
import time
from . import config, sensors, power_profiles, epp, utils

DEBUG = "--debug" in sys.argv

def debug_print(msg):
    if DEBUG:
        print(f"[debug] {msg}")

def run():
    print("dynamic_power: starting daemon loop")

    cfg = config.Config(debug=DEBUG)

    power_profiles.set_profile("power-saver")
    epp.detect_supported_modes()

    poll_interval = cfg.data.get("general", {}).get("poll_interval", 1)

    while True:
        cfg.reload_if_needed()

        override = utils.get_process_override(cfg.data)
        if override:
            debug_print(f"Override matched: {override}")
            profile = override.get("active_profile")
            epp_value = override.get("active_epp")

            if profile:
                power_profiles.set_profile(profile)
            if epp_value:
                epp.set_epp(epp_value)
        else:
            power_source = sensors.get_power_source()
            debug_print(f"Detected power source: {power_source}")

            load_level = sensors.get_load_level(cfg.data)
            debug_print(f"Load level: {load_level}")

            profile = cfg.get_profile(load_level, power_source)
            epp_value = cfg.get_epp(profile)

            debug_print(f"Setting profile to {profile} and EPP to {epp_value}")
            power_profiles.set_profile(profile)
            epp.set_epp(epp_value)

        time.sleep(poll_interval)
