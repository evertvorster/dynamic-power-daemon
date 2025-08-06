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

async def main():
    logging.info("Starting minimal dynamic_power_command")
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
