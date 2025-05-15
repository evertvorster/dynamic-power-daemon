# Dynamic Power Daemon

A minimalist Linux daemon that **intelligently switches power profiles** according to:

* live CPU load  
* whether the machine is on AC or battery  
* the presence of user-defined **quiet** (low‑noise) or **responsive** (snappy‑desktop) workloads  

The project ships with:

* **`dynamic_power.sh`** – the daemon (run by systemd)  
* **`dynamic_power_monitor.sh`** – an interactive, real‑time dashboard  

---

## Key features

| Feature | Details |
|---------|---------|
| **Load‑aware switching** | Jumps between `power-saver`, `balanced`, and `performance` based on 1‑minute load averages. |
| **Battery‑first safety** | Locks to `power-saver` the moment the machine is unplugged. |
| **Quiet / Responsive modes** | *Quiet* keeps fans silent by never leaving `power-saver`.<br>*Responsive* keeps the desktop perky by never dropping below `balanced`. Each mode watches a **comma‑separated list of processes** you choose. |
| **EPP tuning** | Optionally pins Intel / AMD *Energy Performance Preference* per profile or per mode. |
| **Dual backend** | Works with either `powerprofilesctl` (kernel ≥ 5.11) **or** `asusctl` on ASUS laptops. |
| **Self‑healing config** | `/etc/dynamic-power.conf` is created on first run and **won’t be overwritten** later. Any missing keys are filled with safe defaults and reported. |
| **Journal & TUI feedback** | Gaps in the config are logged to `journalctl` and highlighted in the monitor. |

---

## Quick install (Arch Linux)

A helper package **`dynamic-power-monitor`** is in the AUR:

```bash
paru -S dynamic-power-monitor        # or yay -S …
```

The PKGBUILD places the daemon, monitor, config template and service file in the right locations.

---

## Manual install (any distro)

1. **Dependencies**

   ```bash
   sudo pacman -S bc power-profiles-daemon    # Arch example
   # or
   sudo apt install bc power-profiles-daemon  # Debian/Ubuntu
   ```

2. **Scripts**

   ```bash
   sudo install -m 755 dynamic_power.sh         /usr/local/bin/
   sudo install -m 755 dynamic_power_monitor.sh /usr/local/bin/
   ```

3. **systemd service**

   ```bash
   sudo install -m 644 dynamic-power.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable --now dynamic-power.service
   ```

That’s it—the first launch seeds `/etc/dynamic-power.conf` with documented defaults.

---

## Configuration

Open `/etc/dynamic-power.conf` in your editor; everything is inline‑commented.  
A minimal example (showing only the most‑changed keys) looks like:

```ini
#############################
#  Dynamic-Power Daemon     #
#############################

# CPU load thresholds (float)
LOW_LOAD=1.0
HIGH_LOAD=2.0
CHECK_INTERVAL=10          # seconds between samples

# Backend: powerprofilesctl | asusctl
POWER_TOOL="powerprofilesctl"

# -------- Special modes --------
# Comma-separated process names (pgrep −x matches)
QUIET_PROCESSES="obs-studio,screenrec"
RESPONSIVE_PROCESSES="kdenlive,steam"

# Optional tweaks when a mode is active
QUIET_EPP="balance_power"          # lower clocks without throttling
RESPONSIVE_MIN_PROFILE="balanced"  # floor while responsive

# -------- EPP per standard profile --------
EPP_POWER_SAVER="power"
EPP_BALANCED="balance_performance"
EPP_PERFORMANCE="performance"
```

**Tip:** remove `ffmpeg` from `QUIET_PROCESSES` if you regularly edit video—otherwise the daemon may prioritise quietness during renders.

Whenever you add or remove keys the daemon:

* logs a warning once (`journalctl -t dynamic_power`)  
* falls back to built‑in defaults so it never crashes  

---

## Usage

### Daemon

```bash
sudo systemctl start   dynamic-power.service
sudo systemctl status  dynamic-power.service
sudo systemctl stop    dynamic-power.service
```

### Real‑time monitor

```bash
dynamic_power_monitor.sh
```

Displays AC state, load, active profile, EPP, and live status of Quiet / Responsive modes.  
Missing‑config keys are highlighted in yellow. Press **q** to quit.

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

Distributed under the **GNU GPL v3 or later**.
