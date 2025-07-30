# dynamic-power-daemon

**Dynamic system performance tuning for Linux laptops and desktops** — responsive power profile switching, user overrides, panel overdrive, and display refresh control.

---

## 🧠 Overview

`dynamic-power-daemon` is a lightweight power management suite for Linux systems that dynamically adjusts:

- **CPU power profiles (performance, balanced, powersave)**
- **Energy Performance Preference (EPP) values**
- **Panel overdrive (Asus laptops)**
- **Display refresh rates**
- **Desktop panel auto-hide (KDE)**

It monitors system load, power source, and running applications to deliver the optimal performance or power saving profile — automatically, or with user control.

---

## ⚙️ Architecture

- **System Daemon** (`dynamic_power`) — runs as root via systemd, monitors load and power source, applies profiles.
- **User Helper** (`dynamic_power_user`) — monitors user processes, sends override requests via DBus.
- **GUI Controller** (`dynamic_power_command`) — Qt-based tray app for real-time monitoring, manual control, and config editing.
- **Session Helper** (`dynamic_power_session_helper`) — launches user components and reacts to power state changes.

---

## 🧩 Features

✅ Dynamic switching based on:
- CPU load averages
- Power source (AC or battery)
- Per-process overrides (e.g. set Steam to "performance")

✅ Power source triggers:
- Enable/disable **panel overdrive** (Asus only)
- Set **display refresh rates** (e.g. 144 Hz → 60 Hz on battery)
- Auto-hide desktop panel in KDE (optional)

✅ Manual override via tray icon:
- One-click switch to Performance / Balanced / Powersave
- Live graph of system load with adjustable thresholds

✅ Configurable with YAML:
- System-wide config: `/etc/dynamic-power.yaml`
- User overrides: `~/.config/dynamic_power/config.yaml`

✅ Clean DBus integration:
- Root daemon only accepts authorized commands
- DBus interface separates system metrics and user requests

---

## 🚀 Installation

### Arch Linux (via AUR)

```bash
yay -S dynamic-power-monitor
```

This will install:
- `dynamic_power` systemd daemon
- All user tools (`dynamic_power_user`, `dynamic_power_command`, etc.)
- Example config templates

### Manual Install

Clone the repo:

```bash
git clone https://github.com/evertvorster/dynamic-power-daemon.git
cd dynamic-power-daemon
sudo make install
```

Enable the root daemon:

```bash
sudo systemctl enable --now dynamic_power.service
```

Start the GUI controller from your user session:

```bash
dynamic_power_command &
```

---

## 📁 Config Structure

### `/etc/dynamic-power.yaml`

```yaml
power:
  low_threshold: 1.0
  high_threshold: 2.0
  power_source:
    ac_id: ADP0
    battery_id: BAT0

features:
  auto_panel_overdrive: true
  screen_refresh: true
  kde_autohide_on_battery: false
```

### `~/.config/dynamic_power/config.yaml`

```yaml
process_overrides:
  - process_name: steam
    active_profile: performance
    priority: 80

features:
  auto_panel_overdrive: true
  screen_refresh: true
```

---

## 🖥 Screenshots

Coming soon — tray UI, graph view, and live refresh rate monitor.

---

## 📦 AUR Package

📦 [`dynamic-power-monitor`](https://aur.archlinux.org/packages/dynamic-power-monitor)

Maintained and released by the author. Updates match GitHub releases.

---

## 📚 Documentation

The project includes detailed markdown docs in `/docs/`:

- `DESIGN_OVERVIEW.md`
- `DBUS_COMMUNICATION.md`
- `FEATURE_TOGGLES.md`
- `WORKFLOW_GUIDE.md`

---

## 🛠 Development

To build from source:

```bash
make
sudo make install
```

To uninstall:

```bash
sudo make uninstall
```

To run in debug mode:

```bash
dynamic_power_session_helper --debug
```

---

## 🧷 Dependencies

The following system packages are required:

- `python`
- `python-dbus`
- `python-psutil`
- `python-pyqt6`
- `python-pyqt6-qt6`
- `python-pyqtgraph`
- `python-pyyaml`
- `python-inotify`
- `python-setproctitle`
- `python-dbus-next`
- `kscreen`
- `asusctl` (optional, for panel overdrive)
- `qt6-base`
- `qt6-tools`

---

## ✨ Credits

Developed and maintained by [Evert Vorster](https://github.com/evertvorster)

Contributions, bug reports, and feature suggestions are welcome!

---

## 📄 License

GPL-3.0 License. See `LICENSE` for details.
