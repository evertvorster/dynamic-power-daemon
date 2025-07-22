#!/usr/bin/env python3
"""
Dynamic‑Power session helper (Phase 3)
* Owns org.dynamic_power.UserBus on the session bus
* Spawns and supervises dynamic_power_user
* Polls basic metrics (load 1 m, AC/BAT, battery %) every 2 s
* Emits PowerStateChanged and now toggles panel over‑drive
"""
import asyncio, logging, os, signal, subprocess, time

# dbus‑next 0.2.x and 0.3.x place MessageBus in dbus_next.aio;
# older hacks used dbus_next.message_bus.  Try the modern path first.
try:
    from dbus_next.aio import MessageBus          # >=0.2
except ImportError:                               # ultra‑old fallback
    from dbus_next.message_bus import MessageBus

from dbus_next.service import ServiceInterface, method, signal as dbus_signal

# Project config ------------------------------------------------------
try:
    from config import Config
except ImportError:                              # when installed as a module
    from dynamic_power.config import Config

LOG = logging.getLogger("dynamic_power_session_helper")
USER_HELPER_CMD = ["/usr/bin/dynamic_power_user"]

# ───────────────────────────────────────── helpers ───
def read_first(path):
    try:
        with open(path, "r") as f:
            return f.read().strip()
    except FileNotFoundError:
        return None

def get_power_source():
    from pathlib import Path
    for ac in Path("/sys/class/power_supply").glob("AC*"):
        if read_first(ac / "online") == "1":
            return "AC"
    return "BAT"

def get_battery_percent():
    from pathlib import Path
    bats = list(Path("/sys/class/power_supply").glob("BAT*"))
    if not bats:
        return None
    try:
        return float(read_first(bats[0] / "capacity"))
    except (TypeError, ValueError):
        return None

async def set_panel_overdrive(enable: bool):
    """Invoke asusctl to switch panel over‑drive on/off."""
    cmd = ["asusctl", "armoury", "panel_overdrive", "1" if enable else "0"]
    proc = await asyncio.create_subprocess_exec(
        *cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT,
    )
    out, _ = await proc.communicate()
    if proc.returncode == 0:
        LOG.info("panel_overdrive set to %s", 1 if enable else 0)
    else:
        LOG.error("panel_overdrive failed (%s): %s",
                  proc.returncode, out.decode().strip())

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
        return self._metrics

    @dbus_signal()
    def PowerStateChanged(self, power_state: 's') -> None:
        pass

    # helper for the sensor loop
    def update_metrics(self, m):
        self._metrics.update(m)

# ───────────────────────────────────────── sensor / supervise ───
async def sensor_loop(iface, cfg):
    last_power = None
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
            iface.PowerStateChanged(power_src)
            last_power = power_src

            # ⟶ toggle panel over‑drive when requested
            od_cfg = cfg.get_panel_overdrive_config()
            if od_cfg.get("enable_on_ac", True):
                await set_panel_overdrive(power_src == "AC")

        await asyncio.sleep(2)

async def spawn_user_helper():
    return await asyncio.create_subprocess_exec(
        *USER_HELPER_CMD,
        stdout=asyncio.subprocess.DEVNULL,
        stderr=asyncio.subprocess.DEVNULL,
    )

async def supervise(proc):
    while True:
        await proc.wait()
        LOG.warning("dynamic_power_user exited (%s); respawning", proc.returncode)
        await asyncio.sleep(3)
        proc = await spawn_user_helper()

# ───────────────────────────────────────── main ───
async def main():
    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s")

    # DBus service
    bus = await MessageBus().connect()
    iface = UserBusIface()
    bus.export("/", iface)
    await bus.request_name("org.dynamic_power.UserBus")

    # Project config
    cfg = Config()          # loads /etc/dynamic-power.yaml
    cfg.load()

    # Spawn sub‑helper
    proc = await spawn_user_helper()

    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    loop.add_signal_handler(signal.SIGTERM, stop.set)
    loop.add_signal_handler(signal.SIGINT, stop.set)

    tasks = [
        asyncio.create_task(sensor_loop(iface, cfg)),
        asyncio.create_task(supervise(proc)),
    ]
    await stop.wait()
    for t in tasks:
        t.cancel()
    if proc.returncode is None:
        proc.terminate()
        await proc.wait()

if __name__ == "__main__":
    asyncio.run(main())
