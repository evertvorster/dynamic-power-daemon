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
- A device tree discovered from the running kernel, with one named row per power knob

Removed or not yet re-implemented:

- Panel overdrive
- Display refresh-rate switching

## PCI Runtime Power Inspection

The root feature editor no longer relies only on static example paths in the config template.

The GUI scans `/sys/devices` for power knobs and builds a tree from what the current machine actually exposes. Nodes belonging to devices where the kernel has runtime power management disabled are left out of the tree, since writing to their `power/control` cannot take effect. In practice this means:

- Each device shows the knobs it actually has, each named for itself:
  - `Runtime PM` — `power/control`, offering `on` and `auto`
  - `Autosuspend delay` — `power/autosuspend_delay_ms`, in milliseconds
  - `Wake` — `power/wakeup`, offering `enabled` and `disabled`
- A knob is only offered when the kernel says it is usable, so a device without autosuspend has no delay row and one that cannot wake the system has no wake row
- PCI rows lead with the bus address and are ordered by it — which is also bus-topology order, since a bridge's secondary bus is always above its primary
- A branch row carries the number of devices in it, such as `0000:00:01.1 (4)`, when there is more than the device's own
- A row starting `@` has a rule; a row starting `*` has rules further down its branch. Neither is shown otherwise, and the addresses stay lined up
- PCI devices are labeled from `lspci -D` when available
- Only devices that can actually be autosuspended are offered

This makes the feature editor much more useful on real hardware, because the relevant paths often differ across machines and kernels.

The daemon still applies whatever is stored in `/etc/dynamic_power.yaml`, but the GUI now helps you discover valid device paths instead of forcing you to find them manually first.

## Configuration

Two config files are involved:

- Daemon config: `/etc/dynamic_power.yaml`
- User config: `~/.config/dynamic_power/config.yaml`

`/etc/dynamic_power.yaml` contains:

The shipped version applies nothing. Every profile option and every root rule starts
disabled, because which values a machine accepts cannot be known in advance - the
valid EPP set depends on the running governor, the driver and the platform. So a
fresh install switches between profiles that do nothing until you configure them.

- Load thresholds used by the daemon
- Profile-to-hardware mappings
- Root-required power feature rules

`~/.config/dynamic_power/config.yaml` contains:

- User threshold preferences pushed to the daemon
- Process override rules
- User-session feature settings

A node is written to `/etc/dynamic_power.yaml` once it is either enabled, or already a rule in the file. Everything nobody has touched is left out, along with tree scaffolding and the intervening branch nodes, so the file stays small — a few dozen entries — instead of recording every device the machine exposes.

Switching a rule off does not remove it: it stays in the file, and is applied again if you switch it back on. Use the inspector's `Delete rule` button to remove one. A rule whose hardware is no longer present is likewise kept rather than silently dropped, and is shown greyed out so you can see it and delete it deliberately.

While the root feature disclaimer has not been accepted, rules are still saved but every one is written switched off, so the file records what you configured without anything being applied to the machine.

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
- `yaml-cpp`
- `systemd`
- `upower`
- `pciutils` for `lspci`-based PCI labeling in the root feature inspector

Optional:

- `libkscreen` — provides `kscreen-doctor`, used to read and set monitor refresh rates

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

## Versioning

There is no version field in the build system. Releases are git tags of the form `v5.MM.P`: a minor bump for features, a patch bump for fixes. Each tag has a matching GitHub Release describing it, and the AUR package builds from that tag's source archive. `git describe --tags` therefore names any working tree exactly.

## Documentation

Additional project documentation lives in [`docs/`](./docs):

- [`docs/dynamic_power_user_manual.md`](./docs/dynamic_power_user_manual.md)
- [`docs/dbus_interface_introspection.txt`](./docs/dbus_interface_introspection.txt)
- [`docs/CHANGELOG.md`](./docs/CHANGELOG.md) — historical, covering the 1.0 Python rewrite; releases since then are recorded as GitHub Releases

## License

GPL-3.0-or-later
