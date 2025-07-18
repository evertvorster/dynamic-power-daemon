import os
import time
from . import config, sensors, power_profiles, epp, utils

def run():
    print("dynamic_power: starting daemon loop")

    cfg = config.Config()

    epp.detect_supported_modes()

    poll_interval = cfg.data.get("general", {}).get("poll_interval", 1)

    while True:
        cfg.reload_if_needed()

        override = utils.get_process_override(cfg.data)
        if override:
            print(f"[override] Active process override: {override.get('name')}")
            profile = override.get("active_profile")
            epp_value = override.get("active_epp")

            if profile:
                power_profiles.set_profile(profile)
            if epp_value:
                epp.set_epp(epp_value)

            time.sleep(poll_interval)
            continue

        power_source = sensors.get_power_source(
            cfg.data.get("power", {}).get("power_source", {}).get("ac_id"),
            cfg.data.get("power", {}).get("power_source", {}).get("battery_id")
        )
        load_level = sensors.classify_load(sensors.get_load_average())
        profile = cfg.get_profile(load_level, power_source)
        epp_value = cfg.get_epp(profile)

        power_profiles.set_profile(profile)
        epp.set_epp(epp_value)

        time.sleep(poll_interval)
