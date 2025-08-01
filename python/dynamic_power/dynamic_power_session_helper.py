#!/usr/bin/env python3
import sys
if "--debug" in sys.argv:
    import os
    os.environ["DYNAMIC_POWER_DEBUG"] = "1"

from dynamic_power.sensors import get_panel_overdrive_status, set_refresh_rates_for_power
import asyncio
from inotify_simple import INotify, flags
import os, logging, signal, time, dbus, psutil, getpass, sys
from pathlib import Path as _P
if os.getenv("DYNAMIC_POWER_DEBUG") == "1":
    logging.basicConfig(level=logging.DEBUG, format="%(levelname)s: %(message)s")
else:
    logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
try:
    import setproctitle
    setproctitle.setproctitle("dynamic_power_session_helper")
except ImportError:
    try:
        import ctypes
        libc = ctypes.CDLL(None)
        libc.prctl(15, b'dynamic_power_session_helper', 0, 0, 0)
    except Exception:
        pass

# Flexible import for dbus‑next across 0.2.x / 0.3.x
try:
    from dbus_next.aio import MessageBus
except ImportError:
    from dbus_next.message_bus import MessageBus     # <0.2 fallback

# dbus‑next
from dbus_next.service import ServiceInterface, method, signal as dbus_signal
from dbus_next import Variant      # NEW – wrap a{sv} values
from dbus_next.constants import BusType

# Project config ------------------------------------------------------
try:
    from config import Config, load_config, is_debug_enabled
except ImportError:
    from dynamic_power.config import Config, load_config, is_debug_enabled
logging.debug("Debug mode in session helper is enabled (via config)")

#To be removed later when not needed. 
USER_HELPER_CMD = ["/usr/bin/dynamic_power_user"]
UI_CMD = ["/usr/bin/dynamic_power_command"]


# ───────────────────────────────────────── helpers ───
def read_first(path):
    try:
        with open(path, "r") as f:
            return f.read().strip()
    except FileNotFoundError:
        return None

def get_power_source():
    """Return 'AC' if any power-supply whose type=='Mains' is online."""
    for dev in _P("/sys/class/power_supply").iterdir():
        if read_first(dev / "type") == "Mains" and read_first(dev / "online") == "1":
            return "AC"
    return "BAT"

def get_battery_percent():
    bats = [d for d in _P("/sys/class/power_supply").iterdir() if read_first(d / "type") == "Battery"]
    if not bats:
        return None
    try:
        return float(read_first(bats[0] / "capacity"))
    except (TypeError, ValueError):
        return None

async def set_panel_overdrive(enable: bool):
    cmd = ["asusctl", "armoury", "panel_overdrive", "1" if enable else "0"]
    logging.info("Running %s", " ".join(cmd))
    try:
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
        )
    except FileNotFoundError as e:
        logging.debug("asusctl not found in PATH (%s); over‑drive toggle skipped", e)
        return
    out, _ = await proc.communicate()
    if proc.returncode == 0:
        logging.info("panel_overdrive set to %s", 1 if enable else 0)
    else:
        logging.debug("panel_overdrive failed (%s): %s", proc.returncode, out.decode().strip())

# ───────────────────────────────────────── DBus iface ───
class UserBusIface(ServiceInterface):
    def __init__(self):
        super().__init__("org.dynamic_power.UserBus")
        self._metrics = {}
        self._process_matches = []

    @method()
    def Ping(self) -> 's':
        return "pong"

    @method()
    def GetMetrics(self) -> 'a{sv}':
        # Wrap every value in a Variant so a{sv} marshals correctly
        return {
            k: (Variant('s', v) if isinstance(v, str)
                else Variant('d', float(v)))
            for k, v in self._metrics.items()
        }

    @dbus_signal()
    def PowerStateChanged(self, power_state: 's') -> None:
        pass

    @method()
    def GetProcessMatches(self) -> 'aa{sv}':  # returns list of dicts
        return [
            {
                "process_name": Variant('s', match.get("process_name", "")),
                "priority":     Variant('i', match.get("priority", 0)),
                "active":       Variant('b', match.get("active", False)),
            }
            for match in self._process_matches
        ]
    
    @method()
    def UpdateProcessMatches(self, matches: 'aa{sv}') -> 'b':
        try:
            unpacked = []
            for item in matches:
                unpacked.append({
                    "process_name": str(item["process_name"].value),
                    "priority": int(item["priority"].value),
                    "active": bool(item["active"].value),
                })
            self._process_matches = unpacked
            logging.debug("Updated process matches via DBus call: %s", unpacked)
            return True
        except Exception as e:
            logging.info(f"[UserBusIface.UpdateProcessMatches] {e}")
            return False


    def update_metrics(self, m):
        #panel_status = get_panel_overdrive_status()
        #logging.debug(f"[DEBUG] Panel overdrive status detected: {panel_status}")
        #self._metrics["panel_overdrive"] = panel_status
        self._metrics.update(m)
        logging.debug("[DEBUG] self._metrics = %s", self._metrics)

    def update_process_matches(self, matches):
        self._process_matches = matches or []
        logging.debug("[DEBUG] self._process_matches = %s", self._process_matches)


# ───────────────────────────────────────── loops ───
async def sensor_loop(iface, cfg_ref, inotify):
    last_power = None
    logging.info("Sensor loop started")
    try:
        while True:
            # Reload config 
            for event in inotify.read(timeout=0):
                if event.mask & flags.MODIFY:
                    cfg_ref["cfg"] = load_config()
                    logging.debug("Config reloaded due to inotify change")

            load1, _, _ = os.getloadavg()
            power_src = get_power_source()
            batt = get_battery_percent()
            try:
                iface.update_metrics({
                    "timestamp": time.time(),
                    "load_1m": load1,
                    "power_source": power_src,
                    **({"battery_percent": batt} if batt is not None else {})
                })
            except Exception as e:
                logging.debug("Failed to update metrics: %s", e)

            # Detects power state changes, then does stuff.
            logging.debug("[DEBUG] last_power = %s", last_power)
            logging.debug("[DEBUG] current power_src = %s", power_src)
            if power_src != last_power:
                logging.debug("Power source change detected: %s → %s", last_power, power_src)
                iface.PowerStateChanged(power_src)
                last_power = power_src

                # Handle the panel overdrive feature
                # Panel Overdrive Logic (simplified – based on single config variable)
                logging.debug("[DEBUG] cfg path = %s", cfg_ref["cfg"].path)
                logging.debug("[DEBUG] cfg panel block = %s", cfg_ref["cfg"].get_panel_overdrive_config())
                if cfg_ref["cfg"].get_panel_overdrive_config().get("enabled", True):
                    logging.debug("[DEBUG] Panel overdrive feature is enabled in config")
                    logging.debug("[DEBUG] Setting panel overdrive to: %s", power_src == "AC")
                    await set_panel_overdrive(power_src == "AC")
                    status = get_panel_overdrive_status()
                    logging.debug("Verified panel_overdrive status: %s", status)
                else:
                    status = "Disabled"
                    logging.debug("[DEBUG] Panel overdrive feature is disabled in config")

                # Refresh Rate Logic (independent toggle)
                logging.debug("[DEBUG] cfg screen_refresh block = %s", cfg_ref["cfg"].get_screen_refresh_config())
                if cfg_ref["cfg"].get_screen_refresh_config().get("enabled", True):
                    logging.debug("[DEBUG] Screen refresh feature is enabled in config")
                    logging.debug("[DEBUG] Setting refresh rates for: %s", power_src)
                    try:
                        set_refresh_rates_for_power(power_src)
                    except Exception as e:
                        logging.info(f"Failed to adjust screen refresh rates: {e}")
                else:
                    logging.debug("[DEBUG] Screen refresh feature is disabled in config")


            await asyncio.sleep(2)
    
    except Exception as e:
        logging.info("Sensor loop crashed: %s", e)  

async def spawn_user_helper():
    return await asyncio.create_subprocess_exec(
        *USER_HELPER_CMD,
    )

async def spawn_ui():
    """Launch dynamic_power_command GUI and return the process object."""
    return await asyncio.create_subprocess_exec(
        *UI_CMD,
    )

async def monitor_ui(proc):
    """Restart UI if it exits unexpectedly."""
    while True:
        await proc.wait()
        logging.info("dynamic_power_command exited (%s); respawning", proc.returncode)
        await asyncio.sleep(3)
        proc = await spawn_ui()


async def supervise(proc):
    while True:
        await proc.wait()
        logging.info("dynamic_power_user exited (%s); respawning", proc.returncode)
        await asyncio.sleep(3)
        proc = await spawn_user_helper()

def system_dbus_service_available(name):
    try:
        bus = dbus.SystemBus()
        return bus.name_has_owner(name)
    except Exception as e:
        logging.info(f"DBus check failed: {e}")
        return False


# ───────────────────────────────────────── main ───
async def main():
    logging.debug("[debug] main() started")
    username = getpass.getuser()
    for proc in psutil.process_iter(['pid', 'name', 'username', 'cmdline']):
        if proc.info['username'] == username and proc.info.get('cmdline'):
            cmd = proc.info['cmdline'][0]
            if cmd == '/usr/bin/dynamic_power':
                logging.info("Detected unexpected user-owned dynamic_power process. Skipping launch.")
                return


    bus = await MessageBus().connect()
    iface = UserBusIface()
    bus.export("/", iface)
    await bus.request_name("org.dynamic_power.UserBus")

    #Changed to get the config to be global in this file.
    from dynamic_power.config import load_config
    cfg_ref = {"cfg": load_config()}
    
    # Wait until the system daemon registers its DBus name
    try:
        for _ in range(10):
            if system_dbus_service_available("org.dynamic_power.Daemon"):
                logging.info("Confirmed: org.dynamic_power.Daemon is available on system bus.")
                break
            logging.info("Waiting for org.dynamic_power.Daemon to appear on DBus...")
            await asyncio.sleep(0.5)
        else:
            logging.info("Timeout waiting for org.dynamic_power.Daemon to register on DBus.")
    except Exception as e:
        logging.info(f"DBus check failed: {e}")


    proc = await spawn_user_helper()
    # Read the panel status just once when starting up. 
    from dynamic_power.sensors import get_panel_overdrive_status
    panel_status = get_panel_overdrive_status()
    iface._metrics["panel_overdrive"] = panel_status
    logging.debug(f"[DEBUG] Panel overdrive status (startup): {panel_status}")

    # Start GUI
    ui_proc = await spawn_ui()


    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    loop.add_signal_handler(signal.SIGTERM, stop.set)
    loop.add_signal_handler(signal.SIGINT, stop.set)

    from inotify_simple import INotify, flags
    import os

    inotify = INotify()
    CONFIG_PATH = os.path.expanduser("~/.config/dynamic_power/config.yaml")
    inotify.add_watch(CONFIG_PATH, flags.MODIFY)

    tasks = [
        asyncio.create_task(sensor_loop(iface, cfg_ref, inotify)),
        asyncio.create_task(supervise(proc)),
        asyncio.create_task(monitor_ui(ui_proc)),
    ]
    await stop.wait()
    for t in tasks:
        t.cancel()
    if proc.returncode is None:
        proc.terminate()
        await proc.wait()
    if ui_proc.returncode is None:
        ui_proc.terminate()
        await ui_proc.wait()

if __name__ == "__main__":
    import asyncio
    asyncio.run(main())