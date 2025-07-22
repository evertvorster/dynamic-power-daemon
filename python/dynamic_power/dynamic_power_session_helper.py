#!/usr/bin/env python3
"""Dynamic‑Power session helper (phase‑2)

* Owns org.dynamic_power.UserBus on the session bus.
* Spawns and supervises one dynamic_power_user subprocess.
* Emits a heartbeat to the journal every 30 s.
"""

import asyncio
import logging
import signal
import subprocess

from dbus_next.aio import MessageBus
from dbus_next.service import ServiceInterface, method

LOG = logging.getLogger("dynamic_power_session_helper")

CMD_USER_HELPER = ["/usr/bin/dynamic_power_user"]

class UserBusIface(ServiceInterface):
    def __init__(self):
        super().__init__("org.dynamic_power.UserBus")

    @method()
    def Ping(self) -> 's':
        return "pong"

async def spawn_user_helper():
    LOG.info("Starting dynamic_power_user")
    proc = await asyncio.create_subprocess_exec(
        *CMD_USER_HELPER,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return proc

async def supervise(proc):
    await proc.wait()
    LOG.warning("dynamic_power_user exited with %s; respawning in 3 s",
                proc.returncode)
    await asyncio.sleep(3)
    return await spawn_user_helper()

async def main():
    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s")

    bus = await MessageBus().connect()
    iface = UserBusIface()
    bus.export("/", iface)
    await bus.request_name("org.dynamic_power.UserBus")
    LOG.info("UserBus name acquired")

    proc = await spawn_user_helper()

    async def heartbeat():
        while True:
            LOG.info("heartbeat (child pid %s)", proc.pid)
            await asyncio.sleep(30)

    stop = asyncio.Event()

    def shutdown():
        LOG.info("Received termination signal")
        stop.set()

    loop = asyncio.get_event_loop()
    loop.add_signal_handler(signal.SIGTERM, shutdown)
    loop.add_signal_handler(signal.SIGINT, shutdown)

    supervisor_task = asyncio.create_task(supervise(proc))
    hb_task = asyncio.create_task(heartbeat())
    await stop.wait()

    supervisor_task.cancel()
    hb_task.cancel()
    if proc.returncode is None:
        proc.terminate()
        await proc.wait()

if __name__ == "__main__":
    asyncio.run(main())
