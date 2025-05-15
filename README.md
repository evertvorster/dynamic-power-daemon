# Dynamic Power Daemon

A minimalist Linux daemon that intelligently switches power profiles based on:

* live CPU load  
* AC vs battery state (auto‑detected)  
* user‑defined **quiet** (fan‑friendly) or **responsive** (snappy) workloads  

This repo contains:

* **`dynamic_power.sh`** – the daemon (systemd unit)  
* **`dynamic_power_monitor.sh`** – an interactive real‑time dashboard  

---

## Key features

| Feature | Details |
|---------|---------|
| **Load‑aware switching** | Slides between `power-saver`, `balanced`, and `performance` using 1‑minute load averages. |
| **Battery‑first safety** | Locks to `power-saver` when unplugged. |
| **Quiet / Responsive modes** | *Quiet* never leaves `power-saver`; *Responsive* never drops below `balanced`. Each mode watches a comma‑separated list of processes. |
| **EPP tuning** | Pin Intel/AMD Energy‑Performance‑Preference per profile or per mode. |
| **Dual backend** | Works with `powerprofilesctl` (kernel ≥ 5.11) **or** `asusctl` on ASUS laptops. |
| **Self‑healing config** | `/etc/dynamic-power.conf` is created at first run. Missing keys auto‑fill with safe defaults and are reported in the journal/TUI. |
| **Smart AC detection** | If `AC_PATH` in the config is wrong, the daemon hunts common paths (`ADP0`, `AC`, `ACAD`, `ACPI`, `MENCHR`, …), picks the first valid one and logs what it chose. |
| **Journal & TUI feedback** | Config gaps go to `journalctl`; the monitor highlights them plus any AC‑path mismatch. |

---

## Quick install (Arch Linux)

```bash
paru -S dynamic-power-monitor        # or yay -S dynamic-power-monitor
```

The AUR package installs the daemon, monitor, config template and service file.

---

## Manual install

### 1 Dependencies

```bash
sudo pacman -S bc power-profiles-daemon          # Arch example
# or
sudo apt  install bc power-profiles-daemon       # Debian/Ubuntu
```

### 2 Scripts

```bash
sudo install -m 755 dynamic_power.sh         /usr/local/bin/
sudo install -m 755 dynamic_power_monitor.sh /usr/local/bin/
```

### 3 systemd service

```bash
sudo install -m 644 dynamic-power.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now dynamic-power.service
```

---

## Configuration

Edit `/etc/dynamic-power.conf` – generated on first run and **never overwritten**.  
Minimal snippet (only keys most folks tweak):

```ini
#############################
#  Dynamic-Power Daemon     #
#############################

# -------- Load thresholds --------
LOW_LOAD=1.0
HIGH_LOAD=2.0
CHECK_INTERVAL=10       # seconds

# -------- Backend --------
POWER_TOOL="powerprofilesctl"      # or "asusctl"

# -------- AC adapter path --------
# Leave empty to let the daemon auto‑detect a valid path under
# /sys/class/power_supply/*/online
# Examples:
#   /sys/class/power_supply/ADP0/online
#   /sys/class/power_supply/AC/online
#   /sys/class/power_supply/ACAD/online
AC_PATH=""

# -------- Special modes --------
QUIET_PROCESSES="obs-studio,screenrec"
RESPONSIVE_PROCESSES="kdenlive,steam"

QUIET_EPP="balance_power"
RESPONSIVE_MIN_PROFILE="balanced"

# -------- EPP per standard profile --------
EPP_POWER_SAVER="power"
EPP_BALANCED="balance_performance"
EPP_PERFORMANCE="performance"
```

**Tip:** remove `ffmpeg` from `QUIET_PROCESSES` if you render video; otherwise quiet mode may override responsiveness during exports.

### What if the path is wrong?

If `AC_PATH` is set but invalid, the daemon:

1. tries common paths and picks the first that works  
2. logs a line like  

   ```
   dynamic_power: Configured AC_PATH invalid; using /sys/class/power_supply/AC/online
   ```

The monitor shows the same message in yellow.

---

## Usage

### Daemon

```bash
sudo systemctl restart dynamic-power.service   # after config edits
journalctl -u dynamic-power.service -f        # live log
```

### Monitor

```bash
dynamic_power_monitor.sh
```

Shows AC state, load, active profile, EPP, Quiet/Responsive status, plus any config warnings. Press **q** to quit.

---

## Uninstall

```bash
sudo systemctl disable --now dynamic-power.service
sudo rm /usr/local/bin/dynamic_power{,_monitor}.sh
sudo rm /etc/dynamic-power.conf
sudo rm /etc/systemd/system/dynamic-power.service
```

---

## License

Released under the **GNU GPL v3 or later**.
