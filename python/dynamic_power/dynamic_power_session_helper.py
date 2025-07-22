#!/usr/bin/env python3
"""
Dynamic‑Power session helper – Phase 1
* Owns org.dynamic_power.UserBus on the session bus
* Spawns and supervises dynamic_power_user
* Polls basic metrics (load 1m, AC/BAT, battery %) every 2 s
* Exposes GetMetrics() method and PowerStateChanged signal
"""
import asyncio, logging, os, signal, subprocess, time
from dbus_next.aio import MessageBus
from dbus_next.service import ServiceInterface, method, signal as dbus_signal

LOG = logging.getLogger("dynamic_power_session_helper")
USER_HELPER_CMD = ["/usr/bin/dynamic_power_user"]

def read_first(path):
    try:
        with open(path, "r") as f:
            return f.readline().strip()
    except FileNotFoundError:
        return None

def get_power_source():
    from pathlib import Path
    for ac in Path("/sys/class/power_supply").glob("AC*"):
        if read_first(ac/"online") == "1":
            return "AC"
    return "BAT"

def get_battery_percent():
    from pathlib import Path
    bats = list(Path("/sys/class/power_supply").glob("BAT*"))
    if not bats:
        return None
    try:
        return float(read_first(bats[0]/"capacity"))
    except (TypeError, ValueError):
        return None

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
    def PowerStateChanged(self, new_state: 's') -> None:
        pass

    # internal
    def update_metrics(self, data):
        self._metrics = data

async def spawn_user_helper():
    LOG.info("Starting dynamic_power_user")
    return await asyncio.create_subprocess_exec(
        *USER_HELPER_CMD,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

async def sensor_loop(iface):
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
        await asyncio.sleep(2)

async def supervise(proc):
    while True:
        await proc.wait()
        LOG.warning("dynamic_power_user exited (%s); respawning", proc.returncode)
        await asyncio.sleep(3)
        proc = await spawn_user_helper()

async def main():
    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s")
    bus = await MessageBus().connect()
    iface = UserBusIface()
    bus.export("/", iface)
    await bus.request_name("org.dynamic_power.UserBus")
    LOG.info("UserBus name acquired")
    proc = await spawn_user_helper()
    stop = asyncio.Event()
    loop = asyncio.get_event_loop()
    loop.add_signal_handler(signal.SIGTERM, stop.set)
    loop.add_signal_handler(signal.SIGINT, stop.set)
    tasks = [
        asyncio.create_task(sensor_loop(iface)),
        asyncio.create_task(supervise(proc)),
    ]
    await stop.wait()
    for t in tasks: t.cancel()
    if proc.returncode is None:
        proc.terminate()
        await proc.wait()

if __name__ == "__main__":
    asyncio.run(main())
