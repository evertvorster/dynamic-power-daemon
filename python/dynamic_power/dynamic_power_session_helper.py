#!/usr/bin/env python3
from dynamic_power.sensors import get_panel_overdrive_status
"""
Dynamic‑Power session helper (Phase 3)
* Owns org.dynamic_power.UserBus on the session bus
* Spawns and supervises dynamic_power_user
* Polls basic metrics (load 1 m, AC/BAT, battery %) every 2 s
* Emits PowerStateChanged and toggles panel over‑drive
"""
import asyncio, logging, os, signal, time
from pathlib import Path as _P

# Flexible import for dbus‑next across 0.2.x / 0.3.x
try:
    from dbus_next.aio import MessageBus
except ImportError:
    from dbus_next.message_bus import MessageBus     # <0.2 fallback

# dbus‑next
from dbus_next.service import ServiceInterface, method, signal as dbus_signal
from dbus_next import Variant      # NEW – wrap a{sv} values

# Project config ------------------------------------------------------
try:
    from config import Config
except ImportError:
    from dynamic_power.config import Config

LOG = logging.getLogger("dynamic_power_session_helper")
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
    LOG.info("Running %s", " ".join(cmd))
    try:
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
        )
    except FileNotFoundError as e:
        LOG.error("asusctl not found in PATH (%s); over‑drive toggle skipped", e)
        return
    out, _ = await proc.communicate()
    if proc.returncode == 0:
        LOG.info("panel_overdrive set to %s", 1 if enable else 0)
    else:
        LOG.error("panel_overdrive failed (%s): %s", proc.returncode, out.decode().strip())

# ───────────────────────────────────────── DBus iface ───
class UserBusIface(ServiceInterface):
    def __init__(self):
        super().__init__("org.dynamic_power.UserBus")
        self._metrics = {}

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

    def update_metrics(self, m):
        panel_status = get_panel_overdrive_status()
        print(f"[DEBUG] Panel overdrive status detected: {panel_status}")  # DEBUG LINE
        self._metrics["panel_overdrive"] = panel_status
        self._metrics.update(m)

# ───────────────────────────────────────── loops ───
async def sensor_loop(iface, cfg):
    last_power = None
    LOG.info("Sensor loop started")
    while True:
        load1, _, _ = os.getloadavg()
        power_src = get_power_source()
        batt = get_battery_percent()

        iface.update_metrics({
            "timestamp": time.time(),
            "load_1m": load1,
            "power_source": power_src,
            **({"battery_percent": batt} if batt is not None else {})
        })

        if power_src != last_power:
            LOG.info("Power source changed %s → %s", last_power, power_src)
            iface.PowerStateChanged(power_src)
            last_power = power_src

            if cfg.get_panel_overdrive_config().get("enable_on_ac", True):
                await set_panel_overdrive(power_src == "AC")

        await asyncio.sleep(2)

async def spawn_user_helper():
    return await asyncio.create_subprocess_exec(
        *USER_HELPER_CMD,
        stdout=asyncio.subprocess.DEVNULL,
        stderr=asyncio.subprocess.DEVNULL,
    )

async def spawn_ui():
    """Launch dynamic_power_command GUI and return the process object."""
    return await asyncio.create_subprocess_exec(
        *UI_CMD,
        stdout=asyncio.subprocess.DEVNULL,
        stderr=asyncio.subprocess.DEVNULL,
    )

async def monitor_ui(proc):
    """Restart UI if it exits unexpectedly."""
    while True:
        await proc.wait()
        LOG.warning("dynamic_power_command exited (%s); respawning", proc.returncode)
        await asyncio.sleep(3)
        proc = await spawn_ui()


async def supervise(proc):
    while True:
        await proc.wait()
        LOG.warning("dynamic_power_user exited (%s); respawning", proc.returncode)
        await asyncio.sleep(3)
        proc = await spawn_user_helper()

# ───────────────────────────────────────── main ───
async def main():
    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(name)s %(message)s")

    bus = await MessageBus().connect()
    iface = UserBusIface()
    bus.export("/", iface)
    await bus.request_name("org.dynamic_power.UserBus")

    cfg = Config()

    proc = await spawn_user_helper()
    # Start GUI
    ui_proc = await spawn_ui()


    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    loop.add_signal_handler(signal.SIGTERM, stop.set)
    loop.add_signal_handler(signal.SIGINT, stop.set)

    tasks = [
        asyncio.create_task(sensor_loop(iface, cfg)),
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
    import asyncio as _asyncio
    _asyncio.run(main())