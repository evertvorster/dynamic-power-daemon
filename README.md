# dynamic-power-daemon

**Dynamic system performance tuning for Linux laptops and desktops** — responsive power profile switching, and configurable user overrides. Keeps your laptop in a low power state when you are not using it, and turns on the power when you are. 

---

## 🧠 Overview

`dynamic-power-daemon` is a lightweight power management suite for Linux systems that dynamically adjusts:

- **CPU power profiles (performance, balanced, powersave)**
- **Panel overdrive (Asus laptops)** <- Removed with a rewrite, will re-implement it
- **Display refresh rates** <- Removed with a rewrite, will re-implement it
- **Desktop panel auto-hide (KDE)** <- Coming soon!

It monitors system load, power source, and running applications to deliver the optimal performance or power saving profile — automatically, or with user control. 

---

## ⚙️ Architecture

- **System Daemon** (`dynamic_power`) — runs as root via systemd, monitors load and power source, applies profiles.
- **User Program** (`dynamic_power_user`) — monitors user processes, sends override requests via DBus. Provides a gui and the controls to your system. You can watch system 
load in real time, and make adjustments to fine tune your experience. Set it once, and the app remembers it until you feel like adjusting it again. 
- **Low power usage** - Optimized to only use power when needed, and stays out of the way if you don't need it.


---

## 🧩 Features

✅ Dynamic switching based on:
- CPU load averages
- Power source (AC or battery)
- Per-process overrides (e.g. set Steam to "performance")

✅ Configurable Daemon power interface:
- CPU Governors, ACPI and ASPM configuration on the fly

✅ Manual override via tray icon:
- One-click switch to Performance / Balanced / Powersave
- Live graph of system load with adjustable thresholds

✅ Configurable with YAML:
- System-wide config: `/etc/dynamic_power.yaml`
- User overrides: `~/.config/dynamic_power/config.yaml`

✅ Clean DBus integration:
- Root daemon only accepts authorized commands
- DBus interface separates system metrics and user requests

---

## 🚀 Installation

### Arch Linux (via AUR)

```bash
yay -S dynamic-power-daemon
```

This will install:
- `dynamic_power` systemd daemon
- All user tools (`dynamic_power_user`)
- Example config templates
- When started for the first time, the daemon and user will copy in a template
    config file. For now the daemon config is manual, config utility coming soon.
- User config has been populated with some examples. Add and remove as you feel like.

### Manual Install

Clone the repo:

```bash
git clone https://github.com/evertvorster/dynamic-power-daemon.git
cd dynamic-power-daemon
cd src
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
sudo cmake --install build

<Yeah, this is stupid, we will fix this soon.>
```

Enable the root daemon:

```bash
sudo systemctl enable --now dynamic_power.service
```

Start the GUI controller from your user session:

```bash
dynamic_power_user &
```
Recommended to add this to your autolaunching scripts. 

---

## 📁 Config Structure

### `/etc/dynamic_power.yaml`

```yaml
thresholds:
  low: 1
  high: 2

grace_period: 15

hardware:
  cpu_governor:
    path: /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
    modes: [powersave, performance]
  acpi_platform_profile:
    path: /sys/firmware/acpi/platform_profile
    modes: [low-power, balanced, performance]
  aspm:
    path: /sys/module/pcie_aspm/parameters/policy
    modes: [powersave, default, performance]

profiles:
  powersave:
    cpu_governor: powersave
    acpi_platform_profile: low-power
    aspm: powersave
  balanced:
    cpu_governor: performance
    acpi_platform_profile: balanced
    aspm: default
  performance:
    cpu_governor: performance
    acpi_platform_profile: performance
    aspm: performance
```

### `~/.config/dynamic_power/config.yaml`

```yaml
features:
  kde_autohide_on_battery: false
  auto_panel_overdrive: true
  screen_refresh: false

power:
  load_thresholds:
    low: 1
    high: 2

process_overrides:
  - name: recording
    process_name: obs
    active_profile: powersave
    priority: 100

  - name: video_editing
    process_name: kdenlive
    active_profile: inhibit powersave
    priority: 90

  - name: gaming
    process_name: steam
    active_profile: performance
    priority: 80

  - name: audio_editing
    process_name: audacity
    active_profile: inhibit powersave
    priority: 70

  - name: arcade_emulation
    process_name: qmc2-sdlmame
    active_profile: performance
    priority: 60

  - name: wine_runtime
    process_name: wineserver
    active_profile: inhibit powersave
    priority: 50
```

---

## 🖥 Screenshots

Coming soon — tray UI, graph view, and live refresh rate monitor.

---

## 📦 AUR Package

📦 [`dynamic-power-daemon`](https://aur.archlinux.org/packages/dynamic-power-daemon)

Maintained and released by the author. Updates match GitHub releases.

---

## 📚 Documentation

The project includes detailed markdown docs in `/docs/`:

- `DESIGN_OVERVIEW.md`
- `DBUS_COMMUNICATION.md`
- `FEATURE_TOGGLES.md`
- `WORKFLOW_GUIDE.md`

---

---

## 🧷 Dependencies

The following system packages are required:
-  'qt6-tools'
-  'kscreen'
-  'cmake'
-  'pkgconf'
-  'qt6-base'
-  'yaml-cpp'
-  'systemd'


optionally:
'asusctl: panel overdrive toggle on Asus laptops'

conflicts:
Anything that wants to set the system power management features. 
Includes but is not limited to power-profiles-control, TLP, Laptop mode tools
    The list goes on. The intention here is to be a modern replacement for these tools.
    DBus interface is available if you want to plug into the profiles, and control them even. 


---

## ✨ Credits

Developed and maintained by [Evert Vorster](https://github.com/evertvorster)

Contributions, bug reports, and feature suggestions are welcome!

---

## 📄 License

GPL-3.0 License. See `LICENSE` for details.
