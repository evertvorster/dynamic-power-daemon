# Dynamic Power User Manual

> Audience: end users
> Applies to: `dynamic_power_user` working with the `dynamic_power` daemon

## Overview

Dynamic Power helps Linux laptops and desktops switch between power-saving and performance-oriented behavior automatically.

The project is split into:

- `dynamic_power`: the root-owned daemon
- `dynamic_power_user`: the Qt tray application and control panel

The daemon watches load and power state, applies the configured hardware profile, and writes any enabled root-level power tweaks. The user app lets you inspect status, set overrides, adjust thresholds, define process rules, and manage optional features.

## Quick Start

1. Install the package.
2. Enable the daemon:

```bash
sudo systemctl enable --now dynamic_power.service
```

3. Launch the user app:

```bash
dynamic_power_user
```

4. Open the main window from the tray icon.
5. Leave the mode on `Dynamic` for normal use unless you need a temporary override.

## What The App Controls

The main window gives you access to four distinct areas:

- User override mode
- Load thresholds
- Process-based overrides
- Power-saving feature editors

## User Override

The override button shows both your requested mode and the daemon's active profile.

- `Dynamic` means the daemon decides based on load and power source.
- `Performance`, `Balanced`, and `Powersave` force a profile.
- `Inhibit Powersave` keeps the daemon from dropping fully into the lowest-power behavior.

If the displayed profile does not change immediately, wait for the next daemon cycle. Load-based decisions run on a 5-second timer.

## Load Average Graph

The graph shows recent system load and the low/high thresholds used by the daemon.

- Below the low threshold: the daemon tends toward `powersave`
- Between low and high: the daemon tends toward `balanced`
- Above the high threshold: the daemon tends toward `performance`

Thresholds changed in the UI are stored in the user config and pushed to the daemon over D-Bus.

## Process Overrides

Process rules let specific applications request a profile automatically.

Typical use cases:

- Raise performance while gaming
- Force balanced mode for development tools
- Keep a specific workload from entering aggressive power saving

Rules live in the user config and are evaluated continuously while the app is running. If more than one rule matches, priority decides which one wins.

## Profile Configuration

The profile configuration editor changes what the daemon writes when a profile is selected.

Current profile capabilities include:

- CPU governor
- CPU EPP policy
- ACPI platform profile
- PCIe ASPM policy

These settings are stored in `/etc/dynamic_power.yaml`.

Important behavior:

- A capability can be set to `disabled` for a specific profile, which means the daemon skips that write.
- CPU governor and EPP paths are expanded across detected CPU policy nodes by the daemon, even if the config shows a single example path.
- Capability paths and options can be checked from the GUI if your kernel exposes them at different locations.

## Power Saving Features

The Power Saving Features dialog has two sections:

- User Power Saving Features
- Root-required features

### User Power Saving Features

These are stored in:

- `~/.config/dynamic_power/config.yaml`

These settings are user-session features and do not require root writes.

### Root-Required Features

These are stored in:

- `/etc/dynamic_power.yaml`

These rules are applied by the root daemon when power state changes.

Each rule can define:

- Whether it is enabled
- The target path
- The value to use on AC
- The value to use on battery
- Whether the node overrides or inherits policy from its parent

The dialog blocks saving until you accept the disclaimer. That is intentional: a bad sysfs write can break device behavior, reduce stability, or increase power use instead of lowering it.

## PCI And Device Runtime Power Controls

The root feature editor now inspects the current machine instead of relying only on static template entries.

The app scans `/sys/devices` for `power/control` files and builds a device tree from what it finds.

This means:

- PCI runtime power entries are discovered dynamically
- PCI devices are labeled from `lspci -D` when `pciutils` is installed
- The usual runtime power values, such as `on` and `auto`, are offered automatically
- The default view is focused on PCI-related nodes
- `Advanced View` reveals non-PCI and other lower-level device nodes

This is the practical workflow for device runtime power tuning:

1. Open the Power Saving Features dialog.
2. Accept the disclaimer if you have not already done so.
3. Browse or search for the target device.
4. Select the `Runtime Power Control` node for that device.
5. Set `Enabled`, then choose AC and battery values.
6. Save the root configuration.
7. Test the result on your own hardware.

The GUI helps you discover valid nodes, but the daemon still persists and applies the final rules from `/etc/dynamic_power.yaml`.

## Configuration Files

There are two important config files:

- `/etc/dynamic_power.yaml`
- `~/.config/dynamic_power/config.yaml`

`/etc/dynamic_power.yaml` contains:

- Daemon thresholds
- Hardware capability paths
- Profile mappings
- Root-required feature rules

`~/.config/dynamic_power/config.yaml` contains:

- User threshold preferences
- Process override rules
- User-session feature settings

Installed templates are usually copied from:

- `/usr/share/dynamic-power/dynamic_power.yaml`
- `/usr/share/dynamic-power/dynamic-power-user.yaml`

## Troubleshooting

### The active profile says `Error` or does not change

Check the journal for the daemon first:

```bash
journalctl -u dynamic_power.service -b
```

Common causes:

- A configured sysfs path does not exist on this machine
- A selected value is not accepted by the kernel
- Another power-management tool is fighting for the same settings

### A root-required rule is not applied

Check the following:

- The disclaimer has been accepted
- The rule is enabled
- The target path exists on this machine
- The AC and battery values are valid for that path

For device runtime power nodes, confirm the target is the actual `.../power/control` file exposed by your kernel.

### PCI devices are shown with generic labels

Install `pciutils` so the GUI can use `lspci -D` for better PCI device descriptions.

### EPP cannot be configured

That usually means the kernel or firmware is not exposing the expected EPP interface. Verify:

- Your CPU/firmware supports it
- The relevant cpufreq driver is active
- The configured path exists on your system

### Threshold changes do not seem to stick

The user app stores thresholds in `~/.config/dynamic_power/config.yaml` and pushes them to the daemon. If they appear to revert, check whether another instance or another config edit overwrote them.

## Practical Advice

- Keep the mode on `Dynamic` unless you have a specific reason not to.
- Add root-required rules one at a time.
- Test every runtime-power change on your hardware before relying on it daily.
- Do not run this alongside TLP, laptop-mode-tools, or other tools writing the same power knobs.

## About

- Project: `dynamic-power-daemon`
- GUI binary: `dynamic_power_user`
- License: GPL-3.0-or-later
