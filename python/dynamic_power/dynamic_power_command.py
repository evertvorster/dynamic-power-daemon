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
from datetime import datetime
from typing import Tuple

from PyQt6 import QtCore, QtWidgets
from qasync import QEventLoop

from dbus_next.aio import MessageBus
from dbus_next.constants import BusType

DEBUG_ENABLED = os.getenv("DYNAMIC_POWER_DEBUG") == "1"
logging.basicConfig(
    level=logging.DEBUG if DEBUG_ENABLED else logging.INFO,
    format="%(levelname)s: [%(name)s] %(message)s",
)
log = logging.getLogger("dynamic_power_command")

# ─────────────────────── DBus helper ───────────────────────────────────────
async def connect_userbus() -> Tuple[MessageBus, object]:
    bus = await MessageBus(bus_type=BusType.SESSION).connect()
    proxy = bus.get_proxy_object(
        "org.dynamic_power.UserBus", "/",
        await bus.introspect("org.dynamic_power.UserBus", "/")
    )
    iface = proxy.get_interface("org.dynamic_power.UserBus")
    log.debug("Connected to org.dynamic_power.UserBus")
    return bus, iface

# ─────────────────────── GUI window ────────────────────────────────────────
class MainWindow(QtWidgets.QWidget):
    def __init__(self, iface):
        super().__init__()
        self._iface = iface

        self.setWindowTitle("Dynamic Power Monitor")
        self.resize(520, 440)

        # Widgets
        self.timestamp_label = QtWidgets.QLabel("Last update: —")
        self.metrics_label   = QtWidgets.QLabel("Metrics:")
        self.override_label  = QtWidgets.QLabel("Override:")
        self.process_list    = QtWidgets.QTextEdit(readOnly=True)

        layout = QtWidgets.QVBoxLayout(self)
        layout.addWidget(self.timestamp_label)
        layout.addWidget(self.metrics_label)
        layout.addWidget(self.override_label)
        layout.addWidget(QtWidgets.QLabel("Matched Processes:"))
        layout.addWidget(self.process_list)

        # Timer → schedules async refresh
        self._qtimer = QtCore.QTimer(self)
        self._qtimer.setInterval(10_000)
        self._qtimer.timeout.connect(self._handle_timer)  # type: ignore[arg-type]
        self._qtimer.start()

    # plain Qt slot — schedules coroutine so it doesn’t block Qt thread
    def _handle_timer(self):
        if DEBUG_ENABLED:
            log.debug("[GUI] timer fired")
        asyncio.create_task(self.refresh())

    # ───────── async refresh ─────────
    async def refresh(self):
        if DEBUG_ENABLED:
            log.debug("[GUI] refresh start")

        # Timestamp
        now = datetime.now().strftime("%H:%M:%S")
        self.timestamp_label.setText(f"Last update: {now}")

        # GetMetrics
        try:
            metrics = await self._iface.call_get_metrics()
            text = "\n".join(f"{k}: {v.value}" for k, v in metrics.items())
            self.metrics_label.setText(f"Metrics:\n{text}")
        except Exception as e:
            self.metrics_label.setText("Metrics: [error]")
            log.debug(f"GetMetrics failed: {e}")

        # GetUserOverride
        try:
            override = await self._iface.call_get_user_override()
            self.override_label.setText(f"Override: {override}")
        except Exception as e:
            self.override_label.setText("Override: [error]")
            log.debug(f"GetUserOverride failed: {e}")

        # GetProcessMatches
        try:
            matches = await self._iface.call_get_process_matches()
            plist = "\n".join(
                f"{m['process_name'].value} (prio={m['priority'].value}) "
                f"→ {'✓' if m['active'].value else '✗'}" for m in matches
            )
            self.process_list.setPlainText(plist)
        except Exception as e:
            self.process_list.setPlainText("[error]")
            log.debug(f"GetProcessMatches failed: {e}")

        # Force paint
        for w in (self.timestamp_label, self.metrics_label, self.override_label, self.process_list):
            w.repaint()
        self.repaint()

        if DEBUG_ENABLED:
            log.debug("[GUI] refresh end")

# ─────────────────────── App bootstrap ─────────────────────────────────────
async def _bootstrap():
    bus, iface = await connect_userbus()

    win = MainWindow(iface)
    await win.refresh()  # initial paint

    # signal handler
    try:
        iface.on_power_state_changed(
            lambda: (log.debug("[GUI] PowerStateChanged"), asyncio.create_task(win.refresh()))
        )
    except TypeError as e:
        log.debug(f"Failed to bind PowerStateChanged: {e}")
    # MetricsUpdated signal (0‑arg)
    try:
        iface.on_metrics_updated(
            lambda: (log.debug("[GUI] MetricsUpdated"), asyncio.create_task(win.refresh()))
        )
    except TypeError as e:
        log.debug(f"Failed to bind MetricsUpdated: {e}")

    QtWidgets.QApplication.instance().aboutToQuit.connect(bus.disconnect)  # type: ignore[arg-type]
    win.show()

def main():
    app = QtWidgets.QApplication(sys.argv)
    loop = QEventLoop(app)
    asyncio.set_event_loop(loop)

    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, app.quit)

    loop.create_task(_bootstrap())
    loop.run_forever()

if __name__ == "__main__":
    main()