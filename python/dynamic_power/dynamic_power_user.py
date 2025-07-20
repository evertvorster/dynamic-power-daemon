#!/usr/bin/env python3
import os
import sys
import time
import yaml
import psutil
import dbus
import signal
from systemd import journal
from setproctitle import setproctitle

DEBUG = "--debug" in sys.argv
CONFIG_PATH = os.path.expanduser("~/.config/dynamic_power/config.yaml")
TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power-user.yaml"

terminate = False
last_seen_processes = set()

def log(msg):
    journal.send(f"dpu_user: {msg}")

def debug(msg):
    if DEBUG:
        journal.send(f"dpu_user (debug): {msg}")

def load_config():
    if not os.path.exists(CONFIG_PATH):
        debug(f"Config file not found at {CONFIG_PATH}, using template.")
        return load_template()
    with open(CONFIG_PATH, "r") as f:
        cfg = yaml.safe_load(f) or {}
    debug("Loaded user config.")
    return cfg

def load_template():
    with open(TEMPLATE_PATH, "r") as f:
        return yaml.safe_load(f) or {}

def send_thresholds(bus, low, high):
    try:
        daemon = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
        iface = dbus.Interface(daemon, "org.dynamic_power.Daemon")
        iface.SetLoadThresholds(float(low), float(high))
        debug(f"Sent thresholds: low={low}, high={high}")
    except Exception as e:
        if DEBUG:
            journal.send(f"dpu_user (error): Failed to send thresholds - {e}")

def check_processes(process_overrides):
    global last_seen_processes
    running = set(p.info["name"] for p in psutil.process_iter(attrs=["name"]))

    if isinstance(process_overrides, list):
        proc_list = {entry.get("process_name"): entry for entry in process_overrides}
    else:
        proc_list = process_overrides

    for proc, data in proc_list.items():
        if proc in running:
            if DEBUG or proc not in last_seen_processes:
                journal.send(f"dpu_user: Detected process {proc}, profile={data.get('profile')}, epp={data.get('epp')}")
            last_seen_processes.add(proc)
        elif proc in last_seen_processes:
            last_seen_processes.remove(proc)

def handle_sigint(signum, frame):
    global terminate
    log("Caught SIGINT, shutting down...")
    terminate = True

def main():
    global terminate

    setproctitle("dynamic_power_user")
    signal.signal(signal.SIGINT, handle_sigint)

    bus = dbus.SystemBus()
    last_mtime = 0
    config = load_config()
    last_mtime = os.path.getmtime(CONFIG_PATH) if os.path.exists(CONFIG_PATH) else 0

    poll_interval = config.get("general", {}).get("poll_interval", 5)
    thresholds = config.get("power", {}).get("load_thresholds", {})
    process_overrides = config.get("process_overrides", {})

    log("dynamic_power_user started.")

    while not terminate:
        try:
            if os.path.exists(CONFIG_PATH):
                mtime = os.path.getmtime(CONFIG_PATH)
                if mtime != last_mtime:
                    config = load_config()
                    poll_interval = config.get("general", {}).get("poll_interval", 5)
                    thresholds = config.get("power", {}).get("load_thresholds", {})
                    process_overrides = config.get("process_overrides", {})
                    last_mtime = mtime
                    debug("Reloaded config due to change.")

            send_thresholds(bus, thresholds.get("low", 1.0), thresholds.get("high", 2.0))
            check_processes(process_overrides)
            time.sleep(poll_interval)
        except Exception as e:
            if DEBUG:
                journal.send(f"dpu_user (error): {e}")
            time.sleep(poll_interval)

    log("dynamic_power_user exited cleanly.")

if __name__ == "__main__":
    main()
