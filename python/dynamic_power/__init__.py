import os
import sys
import time
import yaml
import signal
from dbus_next import Variant
from . import config, sensors, power_profiles, dbus_interface
from .debug import info_log, debug_log, error_log, DEBUG_ENABLED
from inotify_simple import INotify, flags
try:
    import setproctitle
    setproctitle.setproctitle('dynamic_power')
except ImportError:
    try:
        import ctypes
        ctypes.CDLL(None).prctl(15, b'dynamic_power', 0, 0, 0)
    except Exception:
        pass


terminate = False
override_profile = None  # Temporary profile override set via DBus
current_poll_interval = 1  # Default, overridden at runtime
current_threshold_low = 1.0
current_threshold_high = 2.0
thresholds_overridden = False  # New flag to track if thresholds were overridden via DBus

def handle_term(signum, frame):
    global terminate
    info_log("main", f"Received signal {signum}, shutting down...")
    terminate = True

signal.signal(signal.SIGTERM, handle_term)
signal.signal(signal.SIGINT, handle_term)

def run():
    global override_profile, current_poll_interval
    global current_threshold_low, current_threshold_high
    global thresholds_overridden

    info_log("main", "dynamic_power: starting daemon loop")

    cfg = config.Config()
    inotify = INotify()
    watch_fd = inotify.add_watch(cfg.path, flags.MODIFY)

    # Start DBus interface
    try:
        iface = dbus_interface.start_dbus_interface()
        iface.register_set_user_profile(lambda p: set_override(p))
        iface.register_set_poll_interval(lambda v: set_poll_interval(v))
        iface.register_set_thresholds(lambda low, high: set_thresholds(low, high))

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

    # Set initial state (for DBus access)
    initial_profile = cfg.get_profile("low", "ac")
    iface.state["active_profile"] = Variant("s", initial_profile)
    iface.state["threshold_low"] = Variant("d", float(current_threshold_low))
    iface.state["threshold_high"] = Variant("d", float(current_threshold_high))


    while not terminate:
        for event in inotify.read(timeout=0):
            if event.mask & flags.MODIFY:
                debug_log("main", "Config file modified – reloading")
                cfg.load()
       
        if not thresholds_overridden:
            thresholds = cfg.get_thresholds()
            current_threshold_low = thresholds.get("low", 1.0)
            current_threshold_high = thresholds.get("high", 2.0)

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
        iface.state["active_profile"] = Variant("s", profile)
        iface.state["threshold_low"] = Variant("d", float(current_threshold_low))
        iface.state["threshold_high"] = Variant("d", float(current_threshold_high))


        # Write system state to /run/user/<uid>/dynamic_power_state.yaml
        ## ---- to be replaced soon
        try:
            uid = os.getuid()
            state_path = f"/run/dynamic_power_state.yaml"
            with open(state_path, "w") as f:
                yaml.safe_dump({
                    "active_profile": profile,
                    "thresholds": {
                        "low": current_threshold_low,
                        "high": current_threshold_high,
                    },
                    "timestamp": time.time()
                }, f)
        except Exception as e:
            error_log("main", f"Failed to write state file: {e}")

        # end of file writing block to be removed. 

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
    global current_threshold_low, current_threshold_high, thresholds_overridden
    current_threshold_low = max(0, float(low))
    current_threshold_high = max(current_threshold_low + 0.1, float(high))
    thresholds_overridden = True
    info_log("main", f"Thresholds updated via DBus: low={current_threshold_low}, high={current_threshold_high}")