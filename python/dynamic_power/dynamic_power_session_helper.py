#!/usr/bin/env python3
"""Stub helper for the dynamic‑power user session.

Now requests the well‑known bus name so external clients can call Ping().
"""
import asyncio
import logging
from dbus_next.aio import MessageBus
from dbus_next.service import ServiceInterface, method

LOG = logging.getLogger("dynamic_power_session_helper")

class UserBusIface(ServiceInterface):
    def __init__(self):
        super().__init__("org.dynamic_power.UserBus")

    @method()
    def Ping(self) -> 's':
        return "pong"

async def main():
    logging.basicConfig(level=logging.INFO,
                        format='%(asctime)s %(levelname)s %(message)s')
    bus = await MessageBus().connect()
    iface = UserBusIface()
    bus.export("/", iface)
    # request the well‑known name
    await bus.request_name("org.dynamic_power.UserBus")
    LOG.info("UserBus name acquired – helper running")
    while True:
        await asyncio.sleep(30)
        LOG.info("heartbeat")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        LOG.info("Exiting on KeyboardInterrupt")
