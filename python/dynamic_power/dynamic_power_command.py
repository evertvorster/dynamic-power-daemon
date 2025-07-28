#!/usr/bin/env python3
import sys
import os
import time
import subprocess
import yaml
import dbus
import sys
DEBUG = '--debug' in sys.argv
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



import signal
signal.signal(signal.SIGINT, signal.SIG_DFL)
import psutil
from pathlib import Path
from datetime import datetime
from PyQt6 import QtWidgets, QtCore, QtGui
import pyqtgraph as pg

try:
    import setproctitle
    setproctitle.setproctitle('dynamic_power_command')
except ImportError:
    pass


CONFIG_PATH = Path.home() / ".config" / "dynamic_power" / "config.yaml"
TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power-user.yaml"
STATE_PATH = Path("/run/dynamic_power_state.yaml")
MATCHES_PATH = Path(f"/run/user/{os.getuid()}/dynamic_power_matches.yaml")
OVERRIDE_PATH = Path(f"/run/user/{os.getuid()}/dynamic_power_control.yaml")
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
    print(f"[debug] Called _save_panel_overdrive with enabled={enabled}")
    print(f"[debug] Target config path: {CONFIG_PATH}")
    try:
        with open(CONFIG_PATH, "r") as f:
            data = yaml.safe_load(f) or {}
            print(f"[debug] Loaded existing config: {data}")
    except FileNotFoundError:
        data = {}
        print("[debug] Config file not found, starting with empty config")

    if not isinstance(data.get("features"), dict):
        data["features"] = {}
        print("[debug] Created new 'features' section")

    data["features"]["auto_panel_overdrive"] = bool(enabled)
    print(f"[debug] Updated config value: {data['features']}")
    os.makedirs(CONFIG_PATH.parent, exist_ok=True)
    with open(CONFIG_PATH, "w") as f:
        yaml.safe_dump(data, f)
        print("[debug] Config successfully written to disk")

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
        print(f"[debug] Toggle clicked – state: {state}")
        auto_enabled = int(state) == QtCore.Qt.CheckState.Checked.value
        print(f"[debug] Resolved auto_enabled = {auto_enabled}")
        self.auto_panel_overdrive_status_label.setText("On" if auto_enabled else "Off")
        if hasattr(self, "config"):
            if not isinstance(self.config.get("features"), dict):
                self.config["features"] = {}
            self.config["features"]["auto_panel_overdrive"] = auto_enabled
        print("[debug] Updating config")
        _save_panel_overdrive(auto_enabled)

    def __init__(self, tray):
        super().__init__()
        # --- Connect to session DBus for metrics ---
        try:
            bus = dbus.SessionBus()
            proxy = bus.get_object('org.dynamic_power.UserBus', '/')
            self._dbus_iface = dbus.Interface(proxy, 'org.dynamic_power.UserBus')
        except Exception as e:
            if DEBUG:
                print('Failed to connect to org.dynamic_power.UserBus:', e)
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
        self.state_timer.timeout.connect(self.update_state)
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

        # Initialize checkbox state from config
        pov_enabled = _load_panel_overdrive()
        self.auto_panel_overdrive_checkbox.setChecked(pov_enabled)
        self.auto_panel_overdrive_status_label.setText("On" if pov_enabled else "Off")

        # Connect toggle handler
        self.auto_panel_overdrive_checkbox.stateChanged.connect(self._on_auto_panel_overdrive_toggled)

        # Placeholder for process monitor buttons
        self.proc_layout = QtWidgets.QVBoxLayout()
        self.proc_group = QtWidgets.QGroupBox("Monitored Processes")
        self.proc_group.setLayout(self.proc_layout)
        layout.addWidget(self.proc_group)

        self.add_proc_button = QtWidgets.QPushButton("Monitor New Process")
        self.add_proc_button.clicked.connect(self.add_process)
        layout.addWidget(self.add_proc_button)

        self.load_config()
        self.debug_mode = "--debug" in sys.argv
        if not self.debug_mode:
            sys.stdout = open(os.devnull, "w")
            sys.stderr = open(os.devnull, "w")
        # dynamic_power_user is now managed by session helper; no local spawn
        self.user_proc = None
        try:
            self.user_proc = subprocess.Popen(cmd,
                stdout=None if self.debug_mode else subprocess.DEVNULL,
                stderr=None if self.debug_mode else subprocess.DEVNULL,
                start_new_session=True)
        except Exception as e:
            print(f"Failed to launch dynamic_power_user: {e}")
        self.low_line = pg.InfiniteLine(pos=self.config.get('power', {}).get('low_threshold', 1.0), angle=0, pen=pg.mkPen('g', width=1), movable=True)
        self.high_line = pg.InfiniteLine(pos=self.config.get('power', {}).get('high_threshold', 2.0), angle=0, pen=pg.mkPen('b', width=1), movable=True)
        self.graph.addItem(self.low_line)
        self.graph.addItem(self.high_line)
        self.low_line.sigPositionChangeFinished.connect(self.on_low_drag_finished)
        self.high_line.sigPositionChangeFinished.connect(self.on_high_drag_finished)

        # Ensure override file exists with default "Dynamic" mode
        override_state = {"manual_override": "Dynamic"}
        try:
            os.makedirs(OVERRIDE_PATH.parent, exist_ok=True)
            with open(OVERRIDE_PATH, "w") as f:
                yaml.dump(override_state, f)
            if self.debug_mode:
                print(f"[debug] Wrote default override state to {OVERRIDE_PATH}")
        except Exception as e:
            if self.debug_mode:
                print(f"[debug] Failed to write override state: {e}")

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

    def update_state(self):

        try:
            # --- Power source via DBus ---
            if hasattr(self, '_dbus_iface') and self._dbus_iface is not None:
                try:
                    metrics = self._dbus_iface.GetMetrics()
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
                except Exception as e:
                    if DEBUG:
                        print("DBus GetMetrics failed:", e)
                    self.power_status_label.setText("Power status: Unknown")
            else:
                self.power_status_label.setText("Power status: Unavailable")
        except Exception as e:
            if DEBUG:
                print("Error updating power status:", e)
        try:
            if STATE_PATH.exists():
                with open(STATE_PATH, "r") as f:
                    state = yaml.safe_load(f) or {}
                    thresholds = state.get("thresholds", {})
                    active_profile = state.get("active_profile", "Unknown")

                    # Update threshold lines
                    if "low" in thresholds:
                        self.low_line.setValue(thresholds["low"])
                    if "high" in thresholds:
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
            print(f"Error reading state file: {e}")

    def set_profile(self, mode):
        self.profile_button.setText(f"Mode: {mode}")
        with open(OVERRIDE_PATH, "w") as f:
            yaml.dump({"manual_override": mode}, f)

    def load_config(self):
        if not CONFIG_PATH.exists():
            os.makedirs(CONFIG_PATH.parent, exist_ok=True)
            if Path(TEMPLATE_PATH).exists():
                with open(TEMPLATE_PATH) as src, open(CONFIG_PATH, "w") as dst:
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
            if self.debug_mode:
                print("[debug] Apply button pressed.")
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
            with open(MATCHES_PATH, "r") as f:
                matches = yaml.safe_load(f) or {}
                match_list = matches.get("matched_processes", matches if isinstance(matches, list) else [])

                self.matched = {}
                for item in match_list:
                    name = item.get("process_name", "").lower()
                    active = item.get("active", False)
                    self.matched[name] = "active" if active else "inactive"
        except Exception as e:
            self.matched = {}

        active_exists = any(state == 'active' for state in self.matched.values())
        if hasattr(self, 'tray') and self.tray is not None:
            self.tray.update_icon(active_exists)

        for i in range(self.proc_layout.count()):
            btn = self.proc_layout.itemAt(i).widget()
            if not isinstance(btn, QtWidgets.QPushButton):
                continue
            name = btn.text().lower()
            state = self.matched.get(name)
            if state == "active":
                btn.setStyleSheet("background-color: #FFD700; color: black;")
            elif state == "inactive":
                btn.setStyleSheet("background-color: #FFFACD;")
            else:
                btn.setStyleSheet("")
    def on_low_drag_finished(self):
        if self.debug_mode:
            print(f"[debug] Low threshold drag finished at: {self.low_line.value()}")
        self.update_thresholds()
    def on_high_drag_finished(self):
        if self.debug_mode:
            print(f"[debug] High threshold drag finished at: {self.high_line.value()}")
        self.update_thresholds()

        with open(CONFIG_PATH, "w") as f:
            yaml.dump(self.config, f)
            self.config["features"] = {}
        self.config["features"]["auto_panel_overdrive"] = enabled
def main():
    # Wait for X display to be ready before starting the app
    import os, time
    from PyQt6.QtGui import QGuiApplication

    max_tries = 20
    for i in range(max_tries):
        if os.getenv("DISPLAY") or os.getenv("WAYLAND_DISPLAY"):
            break
        print("[startup] Waiting for DISPLAY or WAYLAND_DISPLAY...")
        time.sleep(0.5)
    else:
        print("[error] DISPLAY not found after delay; exiting")
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