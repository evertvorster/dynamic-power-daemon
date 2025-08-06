#!/usr/bin/env python3

import asyncio
import signal
import logging
import os
import sys

# Detect debug flag from environment
DEBUG_ENABLED = os.getenv("DYNAMIC_POWER_DEBUG") == "1"

# Set process name for debugging and tree tracing
try:
    import setproctitle
    setproctitle.setproctitle("dynamic_power_command")
except ImportError:
    try:
        import ctypes
        libc = ctypes.CDLL(None)
        libc.prctl(15, b'dynamic_power_command', 0, 0, 0)
    except Exception:
        pass

# Configure logging
logging.basicConfig(
    level=logging.DEBUG if DEBUG_ENABLED else logging.INFO,
    format="%(levelname)s: %(message)s"
)

if DEBUG_ENABLED:
    logging.debug("[GUI][main]: Debug mode enabled")

# ───────────────────────────────────────── DBus Setup ───
from dbus_next.aio import MessageBus
from dbus_next.constants import BusType

dbus_iface = None  # global reference for use later

async def connect_to_userbus():
    global dbus_iface
    try:
        bus = await MessageBus(bus_type=BusType.SESSION).connect()
        introspection = await bus.introspect("org.dynamic_power.UserBus", "/")
        obj = bus.get_proxy_object("org.dynamic_power.UserBus", "/", introspection)
        iface = obj.get_interface("org.dynamic_power.UserBus")
        dbus_iface = iface

        # Optional signal hookup
        def handle_power_change(state):
            logging.debug(f"[GUI][signal] PowerStateChanged: {state}")
            try:
                iface.on_power_state_changed(lambda s: handle_power_change(s))
            except TypeError as e:
                logging.debug(f"[GUI][signal] Could not bind PowerStateChanged (fallback triggered): {e}")


        logging.debug("[GUI][dbus] Connected to org.dynamic_power.UserBus")

    except Exception as e:
        logging.info(f"[GUI][dbus] Failed to connect to DBus: {e}")

# ───────────────────────────────────────── Main ───
async def main():
    logging.info("Starting dynamic_power_command")
    await connect_to_userbus()

    stop_event = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, stop_event.set)

    await stop_event.wait()
    logging.info("Shutting down cleanly")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except Exception as e:
        logging.error(f"Unexpected exception: {e}")
        sys.exit(1)
