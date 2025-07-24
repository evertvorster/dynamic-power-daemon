# Dynamic Power Daemon

A minimalist Linux daemon that **automatically switches power profiles**
(`power-saver`, `balanced`, `performance`) according to:

* live CPU load  
* AC vs battery state (auto‑detected)  
* user‑defined **quiet** (fan‑friendly) or **responsive** (snappy) workloads  
* **runtime override file** you can toggle with one key press

This repo ships:

* **`dynamic_power.sh`** – system‑wide daemon (systemd unit)  
* **`dynamic_power_monitor.sh`** – hot‑key dashboard & status UI  

---

## What’s new in v3.2 (2025‑05‑16)

| Change | Details |
|--------|---------|
| **Live override** | `/run/dynamic-power.override` (tmpfs) <br>Write `performance`, `balanced`, `power-saver`, or `dynamic` to lock/unlock the profile. |
| **Monitor hot‑keys** | Inside the dashboard press <kbd>p</kbd>=perf, <kbd>b</kbd>=balanced, <kbd>s</kbd>=powersave, <kbd>d</kbd>=dynamic, <kbd>q</kbd>=quit. |
| **Wheel‑writable** | File is `root:wheel 0664`, so any wheel/sudo user can change it without sudo. |
| **Zero disk wake‑ups** | Override lives in `/run` (tmpfs) and is polled along with AC/load. |

---

## Key features (unchanged)

| Feature | Details |
|---------|---------|
| **Load‑aware switching** | Slides between the three profiles using 1‑minute load averages. |
| **Battery‑first safety** | Forces `power-saver` when unplugged. |
| **Powersave / Responsive modes** | *Powersave* never leaves `power‑saver`; *Responsive* never drops below `balanced`. Each mode watches a CSV process list. |
| **Dual backend** | Works with `powerprofilesctl` (kernel ≥ 5.11) **or** `asusctl` on ASUS laptops. |
| **Self‑healing config** | `/etc/dynamic-power.conf` is created on first run; missing keys auto‑fill and are logged. |
| **Smart AC detection** | If `AC_PATH` is wrong, the daemon hunts common paths (`ADP0`, `AC`, `ACAD`, …) and picks a working one. |

---

## Quick install (Arch Linux)

```bash
paru -S dynamic-power-monitor        # or yay -S dynamic-power-monitor
```

The AUR package installs scripts, service and enables + starts it automatically.

---

## Manual install

```bash
sudo pacman -S bc power-profiles-daemon  # deps

sudo install -m755 dynamic_power.sh         /usr/local/bin/
sudo install -m755 dynamic_power_monitor.sh /usr/local/bin/
sudo install -m644 dynamic-power.service    /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now dynamic-power.service
```

---

## Runtime override

| Action | Command | Takes effect |
|--------|---------|--------------|
| Force **performance** | `echo performance > /run/dynamic-power.override` | ≤ `CHECK_INTERVAL` sec |
| Return to **dynamic** | `echo dynamic > /run/dynamic-power.override` | idem |
| Use the monitor | press **p / b / s / d** | instant write |

The file resets on reboot (tmpfs).  

Wheel/sudo group members may write without sudo; others can `sudo tee`.

---

## Configuration file (`/etc/dynamic-power.conf`)

Created once, never overwritten.  
Snippet:

```ini
# thresholds
LOW_LOAD=1.0
HIGH_LOAD=2.0
CHECK_INTERVAL=10

# leave blank to auto-detect adapter path
AC_PATH=""

# backend: powerprofilesctl|asusctl
POWER_TOOL="powerprofilesctl"

# special modes
QUIET_PROCESSES="obs-studio"
RESPONSIVE_PROCESSES="kdenlive"

QUIET_EPP="balance_power"
RESPONSIVE_MIN_PROFILE="balanced"
```

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

GPL‑3.0‑or‑later


## Desktop Integration

Dynamic‑Power now ships a **`dynamic-power.desktop`** entry and an **SVG icon**.

* The desktop file is installed to `/usr/share/applications`, so *Dynamic Power* shows up in KDE, GNOME, etc.
* The SVG icon (`dynamic-power.svg`) is installed to `/usr/share/pixmaps`.
* Users can enable autostart from their DE’s settings; the entry runs `dynamic_power_session_helper`, which in turn starts the tray UI.

## Panel Over‑Drive

On ASUS ROG laptops, the daemon automatically toggles panel over‑drive (via `asusctl armoury panel_overdrive`) when switching between AC and battery.  
This feature is enabled by default and can be configured under `panel.overdrive` in the YAML config.
