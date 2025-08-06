#!/usr/bin/env python3
import sys
import os
import time
import subprocess
import yaml
import dbus
import sys
import logging
import asyncio
from dynamic_power.config import is_debug_enabled
from dynamic_power.user_dbus_interface import UserBusClient
logging.basicConfig(
    level=logging.DEBUG if is_debug_enabled() else logging.INFO,
    format="%(levelname)s: %(message)s"
)
logging.debug("[GUI][Debug]:Debug mode in dynamic_power_command is enabled (via config)")

try:
    import setproctitle
    setproctitle.setproctitle('dynamic_power_command')
except ImportError:
    # Fallback to prctl if setproctitle is unavailable
    try:
        import ctypes
        libc = ctypes.CDLL(None)
        libc.prctl(15, b'dynamic_power_command', 0, 0, 0)
    except Exception:
        pass
from dynamic_power.config import load_user_config, save_user_config
from dynamic_power import sensors


import signal
signal.signal(signal.SIGINT, signal.SIG_DFL)
import psutil
from pathlib import Path
from datetime import datetime
from PyQt6 import QtWidgets, QtCore, QtGui
import qasync
import pyqtgraph as pg


CONFIG_PATH = Path.home() / ".config" / "dynamic_power" / "config.yaml"
TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power-user.yaml"
OVERRIDE_PATH = Path(f"/run/user/{os.getuid()}/dynamic_power_control.yaml")
USER_HELPER_CMD = ["/usr/bin/dynamic_power_user"]
# --- Panel Overdrive Config Helpers ---
def _load_panel_overdrive():
    logging.debug("[GUI][_load_panel_overdrive]")
    """Return boolean for features.panel_overdrive in user config.
    Defaults to False if key or file missing."""
    try:
        with open(CONFIG_PATH, "r") as f:
            data = yaml.safe_load(f) or {}
        return bool(data.get("features", {}).get("auto_panel_overdrive", False))
    except FileNotFoundError:
        return False

def _save_panel_overdrive(enabled: bool):
    """Persist features.panel_overdrive to user config file."""
    logging.debug(f"[GUI][_save_panel_overdrive] Called _save_panel_overdrive with enabled={enabled}")
    logging.debug(f"[GUI][_save_panel_overdrive] Target config path: {CONFIG_PATH}")
    try:
        with open(CONFIG_PATH, "r") as f:
            data = yaml.safe_load(f) or {}
            logging.debug(f"[GUI][_save_panel_overdrive] Loaded existing config: {data}")
    except FileNotFoundError:
        data = {}
        logging.info("[GUI][_save_panel_overdrive] Config file not found, starting with empty config")

    if not isinstance(data.get("features"), dict):
        data["features"] = {}
        logging.info("[GUI][_save_panel_overdrive] Created new 'features' section")

    data["features"]["auto_panel_overdrive"] = bool(enabled)
    logging.info(f"[GUI][_save_panel_overdrive] Updated config value: {data['features']}")
    os.makedirs(CONFIG_PATH.parent, exist_ok=True)
    with open(CONFIG_PATH, "w") as f:
        yaml.safe_dump(data, f)
        logging.info("[GUI][_save_panel_overdrive] Config successfully written to disk")

def asusd_is_running():
    logging.debug("[GUI][asusd_is_running]")
    """Check if the Asus DBus service is available (asusd running)."""
    try:
        bus = dbus.SystemBus()
        return bus.name_has_owner("xyz.ljones.Asusd")
    except Exception:
        return False

class PowerCommandTray(QtWidgets.QSystemTrayIcon):
    def __init__(self, icon, app):
        super().__init__(icon)
        logging.debug("[GUI][__init__(self, icon, app)]")
        # store default and active icons
        self.default_icon = icon
        self.active_icon = self.create_color_icon(QtGui.QColor('#FFD700'))  # yellow when matched
        self.setIcon(self.default_icon)
        # Load all six icons
        self.icon_map = {
            "dynamic-ac": QtGui.QIcon.fromTheme("dynamic_power.default_ac"),
            "dynamic-bat": QtGui.QIcon.fromTheme("dynamic_power.default_battery"),
            "process-match-ac": QtGui.QIcon.fromTheme("dynamic_power.match_ac"),
            "process-match-bat": QtGui.QIcon.fromTheme("dynamic_power.match_battery"),
            "user-override-ac": QtGui.QIcon.fromTheme("dynamic_power.override_ac"),
            "user-override-bat": QtGui.QIcon.fromTheme("dynamic_power.override_battery"),
        }
        self.app = app
        self.menu = QtWidgets.QMenu()
        self.action_open = self.menu.addAction("Open Dynamic Power Command")
        self.action_quit = self.menu.addAction("Quit")
        self.setContextMenu(self.menu)
        # Create the main window and connect actions/signals
        self.window = MainWindow(self)
        self.action_open.triggered.connect(self.show_window)
        self.action_quit.triggered.connect(app.quit)
        self.activated.connect(self.icon_activated)
    def create_color_icon(self, color):
        logging.debug("[GUI][create_color_icon]")
        pixmap = QtGui.QPixmap(16, 16)
        pixmap.fill(color)
        return QtGui.QIcon(pixmap)

    def update_icon(self, active: bool):
        logging.debug("[GUI][update_icon]")
        """Switch tray icon based on process match state."""
        if active:
            self.setIcon(self.active_icon)
        else:
            self.setIcon(self.default_icon)

    def set_icon_by_name(self, name):
        #logging.debug("[GUI][set_icon_by_name]")
        icon = self.icon_map.get(name)
        if icon and not icon.isNull():
            self.setIcon(icon)
        else:
            self.setIcon(self.default_icon)

    def icon_activated(self, reason):
        logging.debug("[GUI][icon_activated]")
        if reason == QtWidgets.QSystemTrayIcon.ActivationReason.Trigger:
            if self.window.isVisible():
                self.window.hide()
            else:
                self.window.show()
                self.window.raise_()
                self.window.activateWindow()
    def show_window(self):
        logging.debug("[GUI][show_window]")
        self.window.show()
        self.window.raise_()
        self.window.activateWindow()

class MainWindow(QtWidgets.QWidget):

    async def async_init(self):
        try:
            self.client = await UserBusClient.connect()
            logging.debug("[GUI] UserBusClient connected")
        except Exception as e:
            logging.info(f"[GUI] Failed to connect to UserBusClient: {e}")
            self.client = None
        # Start timers only after DBus client is ready
        self.timer.timeout.connect(lambda: asyncio.create_task(self.update_ui_state()))
        self.state_timer.timeout.connect(lambda: asyncio.create_task(self.update_ui_state()))
        self.match_timer.timeout.connect(lambda: asyncio.create_task(self.update_process_matches()))

 
    def update_tray_icon(self):
        #logging.debug("[GUI][update_tray_icon]")
        if not hasattr(self, "tray"):
            return
        state = self.current_state
        power = (state.get("power_source") or "ac").lower()
        if state.get("user_override", "Dynamic") != "Dynamic":
            name = f"user-override-{power}"
        elif state.get("process_matched"):
            name = f"process-match-{power}"
        else:
            name = f"dynamic-{power}"
        self.tray.set_icon_by_name(name)

    def _on_auto_panel_overdrive_toggled(self, state):
        logging.debug("[GUI][_on_auto_panel_overdrive_toggled]")
        auto_enabled = int(state) == QtCore.Qt.CheckState.Checked.value
        logging.debug(f"[GUI][_on_auto_panel_overdrive_toggled] Resolved auto_enabled = {auto_enabled}")
        self.auto_panel_overdrive_status_label.setText("On" if auto_enabled else "Off")
        if hasattr(self, "config"):
            if not isinstance(self.config.get("features"), dict):
                self.config["features"] = {}
            self.config["features"]["auto_panel_overdrive"] = auto_enabled
            logging.debug("[GUI][_on_auto_panel_overdrive_toggled] Updating config")
        _save_panel_overdrive(auto_enabled)

    def _on_auto_refresh_toggled(self, state):
        logging.debug("[GUI][_on_auto_refresh_toggled]")
        auto_enabled = int(state) == QtCore.Qt.CheckState.Checked.value
        logging.debug(f"[GUI][_on_auto_refresh_toggled] Resolved auto_refresh_enabled = {auto_enabled}")
        try:
            with open(CONFIG_PATH, "r") as f:
                data = yaml.safe_load(f) or {}
                logging.debug(f"[GUI][_on_auto_refresh_toggled] Loaded existing config for refresh toggle: {data}")
        except FileNotFoundError:
            data = {}
            logging.info("[GUI][_on_auto_refresh_toggled] Config file not found, starting with empty config for refresh toggle")

        if not isinstance(data.get("features"), dict):
            data["features"] = {}
            logging.info("[GUI][_on_auto_refresh_toggled] Created new 'features' section for refresh toggle")

        data["features"]["screen_refresh"] = bool(auto_enabled)
        os.makedirs(CONFIG_PATH.parent, exist_ok=True)
        with open(CONFIG_PATH, "w") as f:
            yaml.safe_dump(data, f)
            logging.info("[GUI][_on_auto_refresh_toggled] Refresh toggle config successfully written to disk")  

    def __init__(self, tray):
        super().__init__()
        self.client = None  # Will be set in async_init
        asyncio.create_task(self.async_init())
        logging.debug("[GUI][__init__(self, tray)]")
        # --- Connect to session DBus for metrics ---

        self.current_state = {"power_source": "AC", "user_override": "Dynamic", "process_matched": False}
        self.tray = tray
        self.setWindowTitle("Dynamic Power Command")
        self.resize(600, 400)
        layout = QtWidgets.QVBoxLayout()
        self.setLayout(layout)

        # Graph area
        self.graph = pg.PlotWidget()
        self.graph.setMinimumHeight(200)
        self.data = [0]*60
        self.ptr = 0
        self.plot = self.graph.plot(self.data, pen='y')
        layout.addWidget(self.graph)

        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update_graph)
        self.timer.start(1000)

        self.state_timer = QtCore.QTimer()
        self.state_timer.start(1000)
        self.match_timer = QtCore.QTimer()
        self.match_timer.start(1000)

        # Power profile button
        self.profile_button = QtWidgets.QPushButton("Dynamic")
        self.profile_menu = QtWidgets.QMenu()
        for mode in ["Dynamic", "Inhibit Powersave", "Performance", "Balanced", "Powersave"]:
            self.profile_menu.addAction(mode, lambda m=mode: self.set_profile(m))
        self.profile_button.setMenu(self.profile_menu)
        layout.addWidget(self.profile_button)
        self.power_status_label = QtWidgets.QLabel('Power status: Unknown')
        self.power_status_label.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        layout.insertWidget(1, self.power_status_label)

        # Panel Overdrive control box just below power status
        self.panel_overdrive_widget = QtWidgets.QWidget()
        pov_layout = QtWidgets.QHBoxLayout()
        pov_layout.setContentsMargins(0, 0, 0, 0)
        self.auto_panel_overdrive_checkbox = QtWidgets.QCheckBox()
        self.auto_panel_overdrive_checkbox.setToolTip("Enable panel overdrive switching")
        self.auto_panel_overdrive_status_label = QtWidgets.QLabel()
        pov_layout.addWidget(self.auto_panel_overdrive_checkbox)
        pov_layout.addWidget(QtWidgets.QLabel("Automatically set Panel Overdrive. Current Panel Overdrive :"))
        pov_layout.addWidget(self.auto_panel_overdrive_status_label)
        pov_layout.addStretch()
        self.panel_overdrive_widget.setLayout(pov_layout)
        layout.insertWidget(2, self.panel_overdrive_widget)
        # Hide Asus-specific UI if asusd is not running
        if not asusd_is_running():
            logging.info("[GUI][__init__] asusd not detected. Hiding panel overdrive controls.")
            self.panel_overdrive_widget.setVisible(False)

        # Display Refresh Rates box just below Panel Overdrive
        # --- Display Refresh Rates row (styled like Panel Overdrive) ---
        self.refresh_widget = QtWidgets.QWidget()
        rr_layout = QtWidgets.QHBoxLayout()
        rr_layout.setContentsMargins(0, 0, 0, 0)

        # toggle placeholder – disabled until switching logic is implemented
        self.auto_refresh_checkbox = QtWidgets.QCheckBox()
        self.auto_refresh_checkbox = QtWidgets.QCheckBox()
        self.auto_refresh_checkbox.setEnabled(True)
        self.auto_refresh_checkbox.setToolTip("Enable screen refresh switching on power change")
        rr_layout.addWidget(self.auto_refresh_checkbox)
        rr_layout.addWidget(QtWidgets.QLabel("Display Refresh Rates:"))

        # single label that will show “eDP-2: 60 Hz,  HDMI-A-1: 144 Hz” etc.
        self.refresh_rates_label = QtWidgets.QLabel("Unavailable")
        rr_layout.addWidget(self.refresh_rates_label)

        rr_layout.addStretch()
        self.refresh_widget.setLayout(rr_layout)
        layout.insertWidget(3, self.refresh_widget)
     
        # Initialize checkbox state from config
        pov_enabled = _load_panel_overdrive()
        self.auto_panel_overdrive_checkbox.setChecked(pov_enabled)
        self.auto_panel_overdrive_status_label.setText("On" if pov_enabled else "Off")
        try:
            with open(CONFIG_PATH, "r") as f:
                data = yaml.safe_load(f) or {}
            auto_refresh_enabled = bool(data.get("features", {}).get("screen_refresh", False))
            self.auto_refresh_checkbox.setChecked(auto_refresh_enabled)
        except FileNotFoundError:
            self.auto_refresh_checkbox.setChecked(False)

        # Connect toggle handler
        self.auto_panel_overdrive_checkbox.stateChanged.connect(self._on_auto_panel_overdrive_toggled)
        self.auto_refresh_checkbox.stateChanged.connect(self._on_auto_refresh_toggled)
        # Placeholder for process monitor buttons
        self.proc_layout = QtWidgets.QVBoxLayout()
        self.proc_group = QtWidgets.QGroupBox("Monitored Processes")
        self.proc_group.setLayout(self.proc_layout)
        layout.addWidget(self.proc_group)

        self.add_proc_button = QtWidgets.QPushButton("Monitor New Process")
        self.add_proc_button.clicked.connect(self.add_process)
        layout.addWidget(self.add_proc_button)

        self.load_config()
        self.low_line = pg.InfiniteLine(pos=self.config.get('power', {}).get('low_threshold', 1.0), angle=0, pen=pg.mkPen('g', width=1), movable=True)
        self.high_line = pg.InfiniteLine(pos=self.config.get('power', {}).get('high_threshold', 2.0), angle=0, pen=pg.mkPen('b', width=1), movable=True)
        self.graph.addItem(self.low_line)
        self.graph.addItem(self.high_line)
        self.low_line.sigPositionChangeFinished.connect(self.on_low_drag_finished)
        self.high_line.sigPositionChangeFinished.connect(self.on_high_drag_finished)
        self.high_line.sigPositionChangeFinished.connect(self.update_thresholds)

    def update_graph(self):
        #logging.debug("[GUI][update_graph(self)]")
        self.ptr += 1
        max_visible = max(self.data)
        y_max = max(7, max_visible + 1)
        self.graph.setYRange(0, y_max)
        load = psutil.getloadavg()[0]
        self.data.append(load)
        self.data = self.data[-60:]
        self.plot.setData(self.data)

    async def update_ui_state(self):
        # Always update power source label, even if not visible
        try:
            metrics = await self.client.get_metrics()
            power_src = metrics.get('power_source', 'Unknown')
            batt = metrics.get('battery_percent', None)
            label = f"Power source: {power_src}"
            if batt is not None:
                label += f" ({batt}%)"
            self.power_status_label.setText(label)
        except Exception as e:
            logging.info(f"[GUI][update_ui_state] Failed to get metrics: {e}")
        #logging.debug("[GUI][update_ui_state(self)]")
        # Always update power source + override + tray icon, even if UI is hidden
        try:
            if self.client is not None:
                metrics = await self.client.get_metrics()
                power_src = metrics.get('power_source', 'Unknown')
                self.current_state["power_source"] = power_src

                requested = "Unknown"
                try:
                    requested = (await self.client.get_user_override()).strip().title()
                except Exception as e:
                    logging.info(f"[GUI][update_ui_state]: Failed to get user override: {e}")

                self.current_state["user_override"] = requested
                self.update_tray_icon()
        except Exception as e:
            logging.info(f"[GUI][update_ui_state]: Failed to update power state for tray: {e}")

        # Skip rest of UI updates if window is hidden
        if not self.isVisible():
            return
        try:
            if self.client is not None:
                # Load config (only if not already loaded)
                auto_enabled = self.config.get("features", {}).get("auto_panel_overdrive", False)
                if not auto_enabled:
                    self.auto_panel_overdrive_status_label.setText("Disabled")
                else:
                    panel = metrics.get('panel_overdrive', None)
                    if panel is not None:
                        self.auto_panel_overdrive_status_label.setText("On" if panel else "Off")
                    else:
                        self.auto_panel_overdrive_status_label.setText("Unknown")
                power_src = metrics.get('power_source', 'Unknown')
                batt = metrics.get('battery_percent', None)
                label = f"Power source: {power_src}"
                if batt is not None:
                    label += f" ({batt}%)"
                self.power_status_label.setText(label)

                # ✅ Smart refresh polling goes here
                now = time.monotonic()
                if not hasattr(self, "_last_refresh_check_time"):
                    self._last_refresh_check_time = 0
                if now - self._last_refresh_check_time >= 10:
                    self._last_refresh_check_time = now
                    current = sensors.get_refresh_info()
                    if current != getattr(self, "_last_refresh_info", None):
                        logging.debug("[GUI][update_ui_state] Detected refresh info change: %s", current)
                        self._last_refresh_info = current.copy()
                        if current:
                            text = ", ".join(
                                f"{screen}: {info.get('current')} Hz"
                                for screen, info in current.items()
                            )
                        else:
                            text = "Unavailable"
                        self.refresh_rates_label.setText(text)
                    else:
                        logging.debug("[GUI][update_ui_state] Refresh info unchanged")
        except Exception as e:
            logging.info(f"DBus GetMetrics or refresh info failed: {e}")

        try:
            bus = dbus.SystemBus()
            daemon = bus.get_object("org.dynamic_power.Daemon", "/org/dynamic_power/Daemon")
            iface = dbus.Interface(daemon, "org.dynamic_power.Daemon")
            state = iface.GetDaemonState()

            thresholds = {
                "low": state.get("threshold_low", 1.0),
                "high": state.get("threshold_high", 2.0),
            }
            active_profile = state.get("active_profile", "Unknown")

            self.low_line.setValue(thresholds["low"])
            self.high_line.setValue(thresholds["high"])

            # Check for manual override
            try:
                requested = "Unknown"
                if hasattr(self, "_dbus_iface") and self._dbus_iface is not None:
                    requested = (await self.client.get_user_override()).strip().title()

                actual = state.get("active_profile", "unknown").replace("-", " ").title()
                self.profile_button.setText(f"Mode: {requested} – {actual}")

                # Highlight manual override modes only
                if requested != "Dynamic":
                    palette = self.profile_button.palette()
                    bg = palette.color(QtGui.QPalette.ColorRole.Highlight)
                    fg = palette.color(QtGui.QPalette.ColorRole.HighlightedText)
                    self.profile_button.setStyleSheet(f"background-color: {bg.name()}; color: {fg.name()};")
                else:
                    self.profile_button.setStyleSheet("")
            except Exception as e:
                logging.info(f"[GUI][update_ui_state] Failed to update profile button: {e}")
                self.profile_button.setText("Mode: Unknown")
        except Exception as e:
            logging.info(f"[GUI][update_ui_state] Failed to read daemon state from DBus: {e}")

    async def set_profile(self, mode):
        logging.debug("[GUI][set_profile(self, mode)]")
        if hasattr(self, "_dbus_iface") and self._dbus_iface is not None:
            try:
                await self.client.set_user_override(mode)
                logging.debug(f"[GUI][set_profile(self, mode)]:Set user override:{mode}")
            except Exception as e:
                logging.info(f"[GUI][set_profile] Failed to set override: {e}")

    def load_config(self):
        logging.debug("[GUI][load_config(self)]")
        if not CONFIG_PATH.exists():
            os.makedirs(CONFIG_PATH.parent, exist_ok=True)
            template_path = (
                "/usr/share/dynamic-power/dynamic-power.yaml"
                if os.geteuid() == 0
                else "/usr/share/dynamic-power/dynamic-power-user.yaml"
            )
            if Path(template_path).exists():
                with open(template_path) as src, open(CONFIG_PATH, "w") as dst:
                    dst.write(src.read())
                    
        with open(CONFIG_PATH, "r") as f:
            self.config = yaml.safe_load(f) or {}

        for i in reversed(range(self.proc_layout.count())):
            widget = self.proc_layout.itemAt(i).widget()
            if widget:
                widget.setParent(None)

        for proc in self.config.get("process_overrides", []):
            name = proc.get("process_name", "Unknown")
            btn = QtWidgets.QPushButton(name)
            btn.clicked.connect(lambda _, p=proc: self.edit_process(p))
            self.proc_layout.addWidget(btn)

    def add_process(self):
        logging.debug("[GUI][add_process(self)]")
        self.edit_process({"process_name": "", "active_profile": "Dynamic", "priority": 0})

    def edit_process(self, proc):
        logging.debug("[GUI][edit_process(self, proc)]")
        dlg = QtWidgets.QDialog(self)
        dlg.setWindowTitle("Edit Process")
        dlg_layout = QtWidgets.QFormLayout()
        dlg.setLayout(dlg_layout)

        name_edit = QtWidgets.QLineEdit(proc.get("process_name", ""))
        dlg_layout.addRow("Process Name", name_edit)

        raw = proc.get("active_profile", "dynamic")
        label = raw.replace("-", " ").title()
        profile_button = QtWidgets.QPushButton(label)       
        profile_menu = QtWidgets.QMenu()
        for mode in ["Dynamic", "Inhibit Powersave", "Performance", "Balanced", "Powersave"]:
            profile_menu.addAction(mode, lambda m=mode: profile_button.setText(m))
        profile_button.setMenu(profile_menu)
        dlg_layout.addRow("Power Mode", profile_button)

        priority_slider = QtWidgets.QSlider(QtCore.Qt.Orientation.Horizontal)
        priority_slider.setRange(0, 100)
        priority_slider.setValue(proc.get("priority", 0))
        dlg_layout.addRow("Priority", priority_slider)

        delete_button = QtWidgets.QPushButton("Delete")
        button_box = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.StandardButton.Apply | QtWidgets.QDialogButtonBox.StandardButton.Close)
        button_row = QtWidgets.QHBoxLayout()
        button_row.addWidget(delete_button)
        button_row.addWidget(button_box)
        dlg_layout.addRow(button_row)

        def apply():
            logging.debug("[GUI][edit_process] Apply button pressed.")
            proc["process_name"] = name_edit.text()
            selected = profile_button.text()
            # Store the profile in lowercase for backend
            proc["active_profile"] = selected.lower()
            proc["priority"] = priority_slider.value()
            self.save_process(proc)
            dlg.accept()

        def delete_proc():
            self.config["process_overrides"] = [
                p for p in self.config.get("process_overrides", [])
                if p.get("process_name") != name_edit.text()
            ]
            with open(CONFIG_PATH, "w") as f:
                yaml.dump(self.config, f)
            self.load_config()
            dlg.accept()

        delete_button.clicked.connect(delete_proc)
        apply_button = button_box.button(QtWidgets.QDialogButtonBox.StandardButton.Apply)
        if apply_button:
            apply_button.clicked.connect(apply)
        button_box.rejected.connect(dlg.reject)
        dlg.exec()

    def save_process(self, proc):
        logging.debug("[GUI][save_process(self, proc)]")
        overrides = self.config.setdefault("process_overrides", [])
        found = False
        for i, p in enumerate(overrides):
            if p.get("process_name") == proc["process_name"]:
                overrides[i] = proc
                found = True
        if not found:
            overrides.append(proc)

        with open(CONFIG_PATH, "w") as f:
            yaml.dump(self.config, f)

        self.load_config()

    def update_thresholds(self):
        logging.debug("[GUI][update_thresholds(self)]")
        new_low = float(self.low_line.value())
        new_high = float(self.high_line.value())
        self.config.setdefault("power", {})
        self.config["power"].setdefault("load_thresholds", {})
        self.config["power"]["load_thresholds"]["low"] = round(new_low, 2)
        self.config["power"]["load_thresholds"]["high"] = round(new_high, 2)
        with open(CONFIG_PATH, "w") as f:
            yaml.dump(self.config, f)

    async def update_process_matches(self):
        #logging.debug("[GUI][update_process_matches(self)]")
        try:
            bus = dbus.SessionBus()
            helper = bus.get_object('org.dynamic_power.UserBus', '/')
            iface = dbus.Interface(helper, 'org.dynamic_power.UserBus')
            matches = iface.GetProcessMatches()
            self.matched = {}
            self.current_state["process_matched"] = False
            for item in matches:
                name = str(item.get("process_name", "")).lower()
                active = bool(item.get("active", False))
                self.matched[name] = "active" if active else "inactive"
        except Exception as e:
            self.matched = {}
            self.current_state["process_matched"] = False
            logging.info(f"[GUI][update_process_matches] Failed to get process matches from DBus: {e}")

        active_exists = any(state == 'active' for state in self.matched.values())
        if hasattr(self, 'tray') and self.tray is not None:
            self.current_state["process_matched"] = active_exists
            self.update_tray_icon()

        for i in range(self.proc_layout.count()):
            btn = self.proc_layout.itemAt(i).widget()
            if not isinstance(btn, QtWidgets.QPushButton):
                continue
            name = btn.text().lower()
            state = self.matched.get(name)
            palette = btn.palette()

            if state == "active":
                bg = palette.color(QtGui.QPalette.ColorRole.Highlight)
                fg = palette.color(QtGui.QPalette.ColorRole.HighlightedText)
                btn.setStyleSheet(f"background-color: {bg.name()}; color: {fg.name()};")

            elif state == "inactive":
                base = palette.color(QtGui.QPalette.ColorRole.Base)
                highlight = palette.color(QtGui.QPalette.ColorRole.Highlight)
                bg = highlight.lighter(140)  # 140% lighter highlight
                fg = palette.color(QtGui.QPalette.ColorRole.Text)
                btn.setStyleSheet(f"background-color: {bg.name()}; color: {fg.name()};")

            else:
                btn.setStyleSheet("")
    def on_low_drag_finished(self):
        logging.debug(f"[GUI][on_low_drag_finished] Low threshold drag finished at: {self.low_line.value()}")
        self.update_thresholds()
    def on_high_drag_finished(self):
        logging.debug(f"[GUI][on_high_drag_finished] High threshold drag finished at: {self.high_line.value()}")
        self.update_thresholds()

    # --- populate the refresh-rate label ---------------------------------
    def update_refresh_rates(self):
        logging.debug("[GUI][update_refresh_rates(self)]")
        try:
            # returns dict or None
            rates = sensors.get_refresh_info() or {}
        except Exception as e:
            logging.info(f"dynamic_power_command: Failed to get refresh rates: {e}")
            rates = {}
        
        logging.debug(f"[GUI][update_refresh_rates]: rates received → {rates}")

        if rates:
            text = ", ".join(
                f"{screen}: {info.get('current')} Hz"
                for screen, info in rates.items()
            )
        else:
            text = "Unavailable"

        self.refresh_rates_label.setText(text)

async def main():
    logging.debug("[GUI][main()]")
    import os
    max_tries = 20
    for i in range(max_tries):
        if os.getenv("DISPLAY") or os.getenv("WAYLAND_DISPLAY"):
            break
        logging.debug("[GUI][main] Waiting for DISPLAY or WAYLAND_DISPLAY...")
        await asyncio.sleep(0.5)
    else:
        logging.info("[GUI][main] DISPLAY not found after delay; exiting")
        return

    app = QtWidgets.QApplication(sys.argv)
    loop = qasync.QEventLoop(app)
    asyncio.set_event_loop(loop)

    icon = QtGui.QIcon.fromTheme("battery")
    tray = PowerCommandTray(icon, app)
    tray.show()

    # Ensure child process is terminated
    def cleanup():
        logging.debug("[GUI][cleanup()]")
        if hasattr(tray.window, "user_proc"):
            tray.window.user_proc.terminate()
            try:
                tray.window.user_proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                tray.window.user_proc.kill()

    app.aboutToQuit.connect(cleanup)

    with loop:
        loop.run_forever()

if __name__ == "__main__":
    import asyncio
    asyncio.run(main())

