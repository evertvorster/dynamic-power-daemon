#!/usr/bin/env python3
"""Stub helper for the dynamic‑power *user‑session* service.

For step‑1 it only:
  • owns the DBus name  org.dynamic_power.UserBus  on the *session* bus
  • exposes a trivial Ping() method so callers can test connectivity
  • writes a heartbeat line to the journal every 30 s
Real user‑override and sensor logic will be integrated later.
"""

import asyncio
import logging

from dbus_next.aio import MessageBus
from dbus_next.service import ServiceInterface, method

LOG = logging.getLogger("dynamic_power_session_helper")

class UserBusIface(ServiceInterface):
    def __init__(self) -> None:
        super().__init__('org.dynamic_power.UserBus')

    # simple echo to prove round‑trip
    @method()
    def Ping(self) -> 's':
        return "pong"

async def main() -> None:
    logging.basicConfig(level=logging.INFO,
                        format='%(asctime)s - %(levelname)s - %(message)s')
    bus = await MessageBus().connect()
    iface = UserBusIface()
    bus.export('/', iface)

    LOG.info("UserBus interface registered — stub helper running")
    while True:
        LOG.info("heartbeat")
        await asyncio.sleep(30)

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        LOG.info("Helper interrupted, exiting")
