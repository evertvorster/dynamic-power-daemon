#!/usr/bin/env python3
"""Dynamic Power Command GUI (dbus‑next, Qt6, qasync).
* Polls `org.dynamic_power.UserBus` every 10 s for Metrics / Override /
  ProcessMatches.
* Also refreshes immediately on the `PowerStateChanged` signal.
* Explicit `repaint()` calls ensure the widgets draw right away.

Set `DYNAMIC_POWER_DEBUG=1` for verbose logs.
"""

import asyncio
import logging
import os
import signal
import sys

try:
    import setproctitle
    setproctitle.setproctitle("dynamic_power_command")
except ImportError:
    try:
        import ctypes
        libc = ctypes.CDLL(None)
        libc.prctl(15, b"dynamic_power_command", 0, 0, 0)
    except Exception:
        pass


DEBUG_ENABLED = os.getenv("DYNAMIC_POWER_DEBUG") == "1"
logging.basicConfig(
    level=logging.DEBUG if DEBUG_ENABLED else logging.INFO,
    format="%(levelname)s: [%(name)s] %(message)s",
)
logging.debug("[GUI][DEBUG]: Debug enabled.")
# ─────────────────────── DBus helper ───────────────────────────────────────
async def connect_userbus() -> Tuple[MessageBus, object]:
    bus = await MessageBus(bus_type=BusType.SESSION).connect()
    proxy = bus.get_proxy_object(
        "org.dynamic_power.UserBus", "/",
        await bus.introspect("org.dynamic_power.UserBus", "/")
    )
    iface = proxy.get_interface("org.dynamic_power.UserBus")
    logging.debug("[GUI][dbus] Connected to org.dynamic_power.UserBus")
    return bus, iface

# ─────────────────────── GUI window ────────────────────────────────────────
class MainWindow(QtWidgets.QWidget):


        # GetProcessMatches
        try:

        except Exception as e:
            self.process_list.setPlainText("[error]")
            logging.debug(f"[GUI] GetProcessMatches failed: {e}")

        # Force paint
        for w in (self.timestamp_label, self.metrics_label, self.override_label, self.process_list):
            w.repaint()
        self.repaint()



    win = MainWindow(iface)
    await win.refresh()  # initial paint

    # signal handler
    try:
        iface.on_power_state_changed(
            lambda s: logging.info(f"[GUI][Signals] PowerStateChanged signal received: {s}")
        )
    except Exception as e:
        logging.info(f"[GUI][Signals] Failed to bind PowerStateChanged handler: {e}")



def main():
    app = QtWidgets.QApplication(sys.argv)


    loop.create_task(_bootstrap())
    loop.run_forever()
    
