#!/usr/bin/env python3
import sys
import os
import time
import subprocess
import yaml
import dbus
import sys
import logging
from dynamic_power.config import is_debug_enabled
logging.basicConfig(
    level=logging.DEBUG if is_debug_enabled() else logging.INFO,
    format="%(levelname)s: %(message)s"
)
logging.debug("Debug mode in dynamic_power_command is enabled (via config)")

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
import pyqtgraph as pg


CONFIG_PATH = Path.home() / ".config" / "dynamic_power" / "config.yaml"
TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power-user.yaml"
OVERRIDE_PATH = Path(f"/run/user/{os.getuid()}/dynamic_power_control.yaml")
USER_HELPER_CMD = ["/usr/bin/dynamic_power_user"]
# --- Panel Overdrive Config Helpers ---
def _load_panel_overdrive():
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
    logging.debug(f"[debug] Called _save_panel_overdrive with enabled={enabled}")
    logging.debug(f"[debug] Target config path: {CONFIG_PATH}")
    try:
        with open(CONFIG_PATH, "r") as f:
            data = yaml.safe_load(f) or {}
            logging.debug(f"[debug] Loaded existing config: {data}")
    except FileNotFoundError:
        data = {}
        logging.info("[debug] Config file not found, starting with empty config")

    if not isinstance(data.get("features"), dict):
        data["features"] = {}
        logging.info("[debug] Created new 'features' section")

    data["features"]["auto_panel_overdrive"] = bool(enabled)
    logging.info(f"[debug] Updated config value: {data['features']}")
    os.makedirs(CONFIG_PATH.parent, exist_ok=True)
    with open(CONFIG_PATH, "w") as f:
        yaml.safe_dump(data, f)
        logging.info("[debug] Config successfully written to disk")

def asusd_is_running():
    """Check if the Asus DBus service is available (asusd running)."""
    try:
        bus = dbus.SystemBus()
        return bus.name_has_owner("xyz.ljones.Asusd")
    except Exception:
        return False

class PowerCommandTray(QtWidgets.QSystemTrayIcon):
    def __init__(self, icon, app):
        super().__init__(icon)
        # store default and active icons
        self.default_icon = icon
        self.active_icon = self.create_color_icon(QtGui.QColor('#FFD700'))  # yellow when matched
        self.setIcon(self.default_icon)

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
        pixmap = QtGui.QPixmap(16, 16)
        pixmap.fill(color)
        return QtGui.QIcon(pixmap)

    def update_icon(self, active: bool):
        """Switch tray icon based on process match state."""
        if active:
            self.setIcon(self.active_icon)
        else:
            self.setIcon(self.default_icon)


    def icon_activated(self, reason):

        if reason == QtWidgets.QSystemTrayIcon.ActivationReason.Trigger:
            if self.window.isVisible():
                self.window.hide()
            else:
                self.window.show()
                self.window.raise_()
                self.window.activateWindow()
    def show_window(self):
        self.window.show()
        self.window.raise_()
        self.window.activateWindow()

class MainWindow(QtWidgets.QWidget):
    def _on_auto_panel_overdrive_toggled(self, state):
        logging.debug(f"[debug] Toggle clicked – state: {state}")
        auto_enabled = int(state) == QtCore.Qt.CheckState.Checked.value
        logging.debug(f"[debug] Resolved auto_enabled = {auto_enabled}")
        self.auto_panel_overdrive_status_label.setText("On" if auto_enabled else "Off")
        if hasattr(self, "config"):
            if not isinstance(self.config.get("features"), dict):
                self.config["features"] = {}
            self.config["features"]["auto_panel_overdrive"] = auto_enabled
            logging.debug("[debug] Updating config")
        _save_panel_overdrive(auto_enabled)

    def _on_auto_refresh_toggled(self, state):
        logging.debug(f"[debug] Refresh toggle clicked – state: {state}")
        auto_enabled = int(state) == QtCore.Qt.CheckState.Checked.value
        logging.debug(f"[debug] Resolved auto_refresh_enabled = {auto_enabled}")
        try:
            with open(CONFIG_PATH, "r") as f:
                data = yaml.safe_load(f) or {}
                logging.debug(f"[debug] Loaded existing config for refresh toggle: {data}")
        except FileNotFoundError:
            data = {}
            logging.info("[debug] Config file not found, starting with empty config for refresh toggle")

        if not isinstance(data.get("features"), dict):
            data["features"] = {}
            logging.info("[debug] Created new 'features' section for refresh toggle")

        data["features"]["screen_refresh"] = bool(auto_enabled)
        os.makedirs(CONFIG_PATH.parent, exist_ok=True)
        with open(CONFIG_PATH, "w") as f:
            yaml.safe_dump(data, f)
            logging.info("[debug] Refresh toggle config successfully written to disk")  

    def __init__(self, tray):
        super().__init__()
        # --- Connect to session DBus for metrics ---
        try:
            bus = dbus.SessionBus()
            proxy = bus.get_object('org.dynamic_power.UserBus', '/')
            self._dbus_iface = dbus.Interface(proxy, 'org.dynamic_power.UserBus')
        except Exception as e:
            logging.debug('Failed to connect to org.dynamic_power.UserBus:', e)
            self._dbus_iface = None

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
        self.state_timer.timeout.connect(self.update_ui_state)
        self.state_timer.start(1000)
        self.match_timer = QtCore.QTimer()
        self.match_timer.timeout.connect(self.update_process_matches)
        self.match_timer.start(1000)

        # Power profile button
        self.profile_button = QtWidgets.QPushButton("Dynamic")
        self.profile_menu = QtWidgets.QMenu()
        for mode in ["Dynamic", "Responsive", "Performance", "Balanced", "Powersave"]:
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
            logging.info("[GUI] asusd not detected. Hiding panel overdrive controls.")
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
        self.ptr += 1
        max_visible = max(self.data)
        y_max = max(7, max_visible + 1)
        self.graph.setYRange(0, y_max)
        load = psutil.getloadavg()[0]
        self.data.append(load)
        self.data = self.data[-60:]
        self.plot.setData(self.data)

    def update_ui_state(self):
        if not self.isVisible():
            return
        try:
            # --- Power source via DBus ---
            if hasattr(self, '_dbus_iface') and self._dbus_iface is not None:
                try:
                    # Load config (only if not already loaded)
                    if not hasattr(self, "config"):
                        self.load_config()

                    # Respect feature toggle
                    metrics = self._dbus_iface.GetMetrics()
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

                    # Only poll kscreen-doctor every 10 seconds
                    if now - self._last_refresh_check_time >= 10:
                        self._last_refresh_check_time = now

                        current = sensors.get_refresh_info()
                        if current != getattr(self, "_last_refresh_info", None):
                            logging.debug("[command] Detected refresh info change: %s", current)
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
                            logging.debug("[command] Refresh info unchanged")
                except Exception as e:
                    logging.info(f"DBus GetMetrics or refresh info failed: {e}")
                    self.power_status_label.setText("Power status: Unknown")
            else:
                self.power_status_label.setText("Power status: Unavailable")
        except Exception as e:
            logging.info(f"Error updating power status: {e}")
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
            if OVERRIDE_PATH.exists():
                with open(OVERRIDE_PATH) as of:
                    override = yaml.safe_load(of) or {}
                    manual = override.get("manual_override", "")
                    if manual and manual != "Dynamic":
                        self.profile_button.setText(f"Mode: {manual}")
                        return

            self.profile_button.setText(f"Mode: Dynamic – {active_profile}")

        except Exception as e:
            logging.info(f"[GUI] Failed to read daemon state from DBus: {e}")

    def set_profile(self, mode):
        self.profile_button.setText(f"Mode: {mode}")
        with open(OVERRIDE_PATH, "w") as f:
            yaml.dump({"manual_override": mode}, f)

    def load_config(self):
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
        self.edit_process({"process_name": "", "active_profile": "Dynamic", "priority": 0})

    def edit_process(self, proc):
        dlg = QtWidgets.QDialog(self)
        dlg.setWindowTitle("Edit Process")
        dlg_layout = QtWidgets.QFormLayout()
        dlg.setLayout(dlg_layout)

        name_edit = QtWidgets.QLineEdit(proc.get("process_name", ""))
        dlg_layout.addRow("Process Name", name_edit)

        profile_button = QtWidgets.QPushButton(proc.get("active_profile", "Dynamic"))
        profile_menu = QtWidgets.QMenu()
        for mode in ["Dynamic", "Responsive", "Performance", "Balanced", "Powersave"]:
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
            logging.debug("[debug] Apply button pressed.")
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
        new_low = float(self.low_line.value())
        new_high = float(self.high_line.value())
        self.config.setdefault("power", {})
        self.config["power"].setdefault("load_thresholds", {})
        self.config["power"]["load_thresholds"]["low"] = round(new_low, 2)
        self.config["power"]["load_thresholds"]["high"] = round(new_high, 2)
        with open(CONFIG_PATH, "w") as f:
            yaml.dump(self.config, f)

    def update_process_matches(self):
        try:
            bus = dbus.SessionBus()
            helper = bus.get_object('org.dynamic_power.UserBus', '/')
            iface = dbus.Interface(helper, 'org.dynamic_power.UserBus')
            matches = iface.GetProcessMatches()
            self.matched = {}
            logging.debug(f"[debug] Process match map: {self.matched}")
            for item in matches:
                name = str(item.get("process_name", "")).lower()
                active = bool(item.get("active", False))
                self.matched[name] = "active" if active else "inactive"
        except Exception as e:
            self.matched = {}
            logging.info(f"[GUI] Failed to get process matches from DBus: {e}")

        active_exists = any(state == 'active' for state in self.matched.values())
        if hasattr(self, 'tray') and self.tray is not None:
            self.tray.update_icon(active_exists)

        for i in range(self.proc_layout.count()):
            btn = self.proc_layout.itemAt(i).widget()
            if not isinstance(btn, QtWidgets.QPushButton):
                continue
            name = btn.text().lower()
            state = self.matched.get(name)
            logging.debug(f"[debug] Checking button '{name}' → state: {state}")
            if state == "active":
                btn.setStyleSheet("background-color: #FFD700; color: black;")
            elif state == "inactive":
                btn.setStyleSheet("background-color: #FFFACD;")
            else:
                btn.setStyleSheet("")
    def on_low_drag_finished(self):
        logging.debug(f"[debug] Low threshold drag finished at: {self.low_line.value()}")
        self.update_thresholds()
    def on_high_drag_finished(self):
        logging.debug(f"[debug] High threshold drag finished at: {self.high_line.value()}")
        self.update_thresholds()

    # --- populate the refresh-rate label ---------------------------------
    def update_refresh_rates(self):
        try:
            # returns dict or None
            rates = sensors.get_refresh_info() or {}
        except Exception as e:
            logging.info(f"dynamic_power_command: Failed to get refresh rates: {e}")
            rates = {}
        
        logging.debug(f"update_refresh_rates(): rates received → {rates}")

        if rates:
            text = ", ".join(
                f"{screen}: {info.get('current')} Hz"
                for screen, info in rates.items()
            )
        else:
            text = "Unavailable"

        self.refresh_rates_label.setText(text)

def main():
    # Wait for X display to be ready before starting the app
    import os, time
    from PyQt6.QtGui import QGuiApplication

    max_tries = 20
    for i in range(max_tries):
        if os.getenv("DISPLAY") or os.getenv("WAYLAND_DISPLAY"):
            break
        logging.debug("[startup] Waiting for DISPLAY or WAYLAND_DISPLAY...")
        time.sleep(0.5)
    else:
        logging.info("[error] DISPLAY not found after delay; exiting")
        return

    app = QtWidgets.QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)

    icon = QtGui.QIcon.fromTheme("battery")
    tray = PowerCommandTray(icon, app)
    tray.show()

    # Ensure child process is terminated
    def cleanup():
        if hasattr(tray.window, "user_proc"):
            tray.window.user_proc.terminate()
            try:
                tray.window.user_proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                tray.window.user_proc.kill()

    app.aboutToQuit.connect(cleanup)

    sys.exit(app.exec())

if __name__ == "__main__":
    main()