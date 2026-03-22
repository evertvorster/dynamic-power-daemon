# dynamic-power-daemon

Dynamic system performance tuning and power saving for Linux laptops and desktops.

`dynamic-power-daemon` combines a root-owned system daemon with a user session GUI. It switches between `performance`, `balanced`, and `powersave` based on load, power source, and user-defined process rules, and it can apply additional sysfs-based power-saving tweaks when AC or battery state changes.

## Overview

The project has two parts:

- `dynamic_power`: a system daemon started by systemd, running as root.
- `dynamic_power_user`: a Qt tray application and control panel running in the user session.

The daemon is responsible for:

- Monitoring system load
- Watching AC/battery state through UPower
- Applying profile-specific hardware settings
- Applying optional root-level sysfs tweaks
- Exposing status and control over D-Bus

The user application is responsible for:

- Manual override controls
- Threshold tuning
- Process-based override rules
- Editing daemon profile mappings
- Inspecting and configuring root-required power features

## Current Feature Set

- Automatic switching between `powersave`, `balanced`, and `performance`
- Per-profile hardware configuration for:
  - CPU governor
  - Energy Performance Preference (EPP)
  - ACPI platform profile
  - PCIe ASPM policy
- Per-process overrides from the user session
- Live load graph and tray controls
- Root-required feature editor for sysfs power-saving knobs
- Dynamic inspection of device runtime power controls under `/sys/devices`

Removed or not yet re-implemented:

- Panel overdrive
- Display refresh-rate switching

## PCI Runtime Power Inspection

The root feature editor no longer relies only on static example paths in the config template.

The GUI now scans `/sys/devices` for writable `power/control` nodes and builds a device tree from what the current machine actually exposes. In practice this means:

- PCI runtime power control entries are discovered dynamically
- PCI devices are labeled from `lspci -D` when available
- Non-PCI device nodes can also be shown in the advanced view
- Detected nodes expose the values the kernel currently accepts, such as `on` and `auto`

This makes the feature editor much more useful on real hardware, because the relevant paths often differ across machines and kernels.

The daemon still applies whatever is stored in `/etc/dynamic_power.yaml`, but the GUI now helps you discover valid device paths instead of forcing you to find them manually first.

## Configuration

Two config files are involved:

- Daemon config: `/etc/dynamic_power.yaml`
- User config: `~/.config/dynamic_power/config.yaml`

`/etc/dynamic_power.yaml` contains:

- Load thresholds used by the daemon
- Profile-to-hardware mappings
- Root-required power feature rules

`~/.config/dynamic_power/config.yaml` contains:

- User threshold preferences pushed to the daemon
- Process override rules
- User-session feature settings

The installed templates are:

- `/usr/share/dynamic-power/dynamic_power.yaml`
- `/usr/share/dynamic-power/dynamic-power-user.yaml`

## Root-Required Features

Root-required features are sysfs or procfs writes that should happen when the machine changes between AC and battery power.

Examples include:

- `/proc/sys/kernel/nmi_watchdog`
- `/sys/module/snd_hda_intel/parameters/power_save`
- `/proc/sys/vm/dirty_writeback_centisecs`
- `/sys/devices/.../power/control`

Each rule can define:

- Whether it is enabled
- The path to write
- The value to use on AC
- The value to use on battery

The daemon applies these rules only when the root feature disclaimer has been accepted in the daemon config.

## Installation

### Arch Linux

From AUR:

```bash
yay -S dynamic-power-daemon
```

### Manual Build

```bash
git clone https://github.com/evertvorster/dynamic-power-daemon.git
cd dynamic-power-daemon/src
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel
sudo cmake --install build
```

Enable the daemon:

```bash
sudo systemctl enable --now dynamic_power.service
```

Start the user application in your session:

```bash
dynamic_power_user &
```

## Dependencies

Build and runtime requirements include:

- `cmake`
- `pkgconf`
- `qt6-base`
- `qt6-tools`
- `yaml-cpp`
- `systemd`
- `upower`
- `pciutils` for `lspci`-based PCI labeling in the root feature inspector

Optional:

- `kscreen`

Conflicts:

- `power-profiles-daemon`
- `tlp`
- `laptop-mode-tools`
- Other tools that try to manage the same sysfs power settings

Running multiple power-management stacks at once is a bad idea. This project expects to own the settings it writes.

## Usage Notes

- The daemon polls load every 5 seconds.
- UPower power-source changes trigger root feature re-application immediately.
- CPU governor and EPP writes are expanded across all detected CPU policy nodes, not just the single example path in the config.
- Profile capability paths can be adjusted from the GUI if your sysfs layout differs from the template.

## Documentation

Additional project documentation lives in [`docs/`](./docs):

- [`docs/dynamic_power_user_manual.md`](./docs/dynamic_power_user_manual.md)
- [`docs/dbus_interface_introspection.txt`](./docs/dbus_interface_introspection.txt)
- [`docs/CHANGELOG.md`](./docs/CHANGELOG.md)

## License

GPL-3.0-or-later
