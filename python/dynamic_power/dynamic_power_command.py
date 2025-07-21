#!/usr/bin/env python3
import sys
import os
import time
import subprocess
import yaml
import signal
import psutil
from pathlib import Path
from datetime import datetime
from PyQt6 import QtWidgets, QtCore, QtGui
import pyqtgraph as pg

CONFIG_PATH = Path.home() / ".config" / "dynamic_power" / "config.yaml"
TEMPLATE_PATH = "/usr/share/dynamic-power/dynamic-power-user.yaml"
STATE_PATH = Path("/run/dynamic_power_user.state.yaml")
OVERRIDE_PATH = Path("/run/dynamic_power_override.yaml")

class PowerCommandTray(QtWidgets.QSystemTrayIcon):
    def __init__(self, icon, app):
        super().__init__(icon)
        self.app = app
        self.menu = QtWidgets.QMenu()
        self.action_open = self.menu.addAction("Open Dynamic Power Command")
        self.action_quit = self.menu.addAction("Quit")
        self.setContextMenu(self.menu)
        self.action_open.triggered.connect(self.show_window)
        self.action_quit.triggered.connect(app.quit)
        self.window = MainWindow()
        self.activated.connect(self.icon_activated)

    def icon_activated(self, reason):
        if reason == QtWidgets.QSystemTrayIcon.ActivationReason.Trigger:
            self.show_window()

    def show_window(self):
        self.window.show()
        self.window.raise_()
        self.window.activateWindow()

class MainWindow(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Dynamic Power Command")
        self.resize(600, 400)
        layout = QtWidgets.QVBoxLayout()
        self.setLayout(layout)

        # Graph area
        self.graph = pg.PlotWidget()
        self.graph.setMinimumHeight(200)
        self.graph.setYRange(0, 10)
        self.data = [0]*60
        self.ptr = 0
        self.plot = self.graph.plot(self.data, pen='y')
        layout.addWidget(self.graph)

        
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update_graph)
        self.timer.start(1000)

        # Power profile button
        self.profile_button = QtWidgets.QPushButton("Mode: Dynamic")
        self.profile_menu = QtWidgets.QMenu()
        for mode in ["Dynamic", "Inhibit Powersave", "Performance", "Balanced", "Powersave"]:
            self.profile_menu.addAction(mode, lambda m=mode: self.set_profile(m))
        self.profile_button.setMenu(self.profile_menu)
        layout.addWidget(self.profile_button)

        # Placeholder for process monitor buttons
        self.proc_layout = QtWidgets.QVBoxLayout()
        self.proc_group = QtWidgets.QGroupBox("Monitored Processes")
        self.proc_group.setLayout(self.proc_layout)
        layout.addWidget(self.proc_group)

        self.add_proc_button = QtWidgets.QPushButton("Monitor New Process")
        self.add_proc_button.clicked.connect(self.add_process)
        layout.addWidget(self.add_proc_button)

        self.load_config()
        # Create draggable threshold lines from config
        self.low_line = pg.InfiniteLine(pos=self.config.get('general', {}).get('low_threshold', 1.0), angle=0, pen=pg.mkPen('g', width=1), movable=True)
        self.high_line = pg.InfiniteLine(pos=self.config.get('general', {}).get('high_threshold', 2.0), angle=0, pen=pg.mkPen('b', width=1), movable=True)
        self.graph.addItem(self.low_line)
        self.graph.addItem(self.high_line)
        self.low_line.sigPositionChanged.connect(self.update_thresholds)
        self.high_line.sigPositionChanged.connect(self.update_thresholds)

    def update_graph(self):
        self.data[self.ptr % len(self.data)] = psutil.getloadavg()[0]
        self.ptr += 1
        self.plot.setData(self.data)

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
            proc["process_name"] = name_edit.text()
            proc["active_profile"] = profile_button.text()
            proc["priority"] = priority_slider.value()
            self.save_process(proc)
            dlg.accept()

        def delete_proc():
            self.config["process_overrides"] = [p for p in self.config.get("process_overrides", []) if p.get("process_name") != name_edit.text()]
            with open(CONFIG_PATH, "w") as f:
                yaml.dump(self.config, f)
            self.load_config()

        # Recreate graph threshold lines
        for line in getattr(self, "_threshold_lines", []):
            self.graph.removeItem(line)
        self._threshold_lines = []

        self.low_line = pg.InfiniteLine(pos=self.config.get("general", {}).get("low_threshold", 1.0),
                angle=0, pen=pg.mkPen('r', width=1), movable=True)
        self.high_line = pg.InfiniteLine(pos=self.config.get("general", {}).get("high_threshold", 2.0),
                angle=0, pen=pg.mkPen('g', width=1), movable=True)
        self.graph.addItem(self.low_line)
        self.graph.addItem(self.high_line)
        self.low_line.sigPositionChanged.connect(self.update_thresholds)
        self.high_line.sigPositionChanged.connect(self.update_thresholds)
        self._threshold_lines = [self.low_line, self.high_line]
        dlg.accept()

        delete_button.clicked.connect(delete_proc)
        button_box.accepted.connect(apply)
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
        self.config.setdefault("general", {})["low_threshold"] = round(new_low, 2)
        self.config["general"]["high_threshold"] = round(new_high, 2)
        with open(CONFIG_PATH, "w") as f:
            yaml.dump(self.config, f)


def main():
    app = QtWidgets.QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)

    icon = QtGui.QIcon.fromTheme("battery")
    tray = PowerCommandTray(icon, app)
    tray.show()

    sys.exit(app.exec())

if __name__ == "__main__":
    main()