# Dynamic Power Daemon

This is a lightweight Linux daemon that dynamically adjusts the system's power profile based on CPU load and system state.

## Features
- Dynamically switches between `power-saver`, `balanced`, and `performance` modes based on CPU load.
- Always stays in `power-saver` mode when on battery power.
- Uses `/sys/class/power_supply/ADP0/online` to detect whether the system is plugged into AC power.
- Supports both `powerprofilesctl` and `asusctl` for power profile management.
- Configurable thresholds and settings via `/etc/dynamic-power.conf`.
- Monitors a configurable process (`obs` by default) for "recording mode", ensuring low-noise operation while adjusting EPP settings.
- Montiors a configurable process ('kdenlive' by default) for "video editing more", ensuring that the CPU does not idle down completely when video editing.
- Includes a separate real-time monitoring utility: `dynamic-power-monitor`.

## Installation

### 1. Install from AUR (Recommended for Arch Linux Users)
The `dynamic-power-monitor` package is available in the AUR. You can install it using an AUR helper like `paru` or `yay`:

```sh
paru -S dynamic-power-monitor
# or
yay -S dynamic-power-monitor
```

If you prefer manual installation, clone the AUR repository and build the package:

```sh
git clone https://aur.archlinux.org/dynamic-power-monitor.git
cd dynamic-power-monitor
makepkg -si
```

### 1. Install Dependencies
Ensure that you have the required dependencies installed. This daemon requires `bc` for floating-point calculations. Install it using:

```sh
sudo pacman -S bc  # For Arch Linux users
```

### 2. Copy the Scripts
Copy the `dynamic_power_daemon.sh` script and monitoring utility to `/usr/local/bin/`:

```sh
sudo cp dynamic_power_daemon.sh /usr/local/bin/
sudo chmod +x /usr/local/bin/dynamic_power_daemon.sh
sudo cp dynamic_power_monitor.sh /usr/local/bin/
sudo chmod +x /usr/local/bin/dynamic_power_monitor.sh
```

### 3. Install the Configuration File
Copy the configuration file to `/etc/`:

```sh
sudo cp dynamic-power.conf /etc/
```

### 4. Install the Systemd Service (Optional)
To have the daemon start automatically, install the systemd service file:

```sh
sudo cp dynamic-power-daemon.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable dynamic-power-daemon.service
sudo systemctl start dynamic-power-daemon.service
```

This will set up the service to run at startup and ensure the daemon is running.

## Configuration

You can adjust load thresholds, power profiles, and EPP settings by editing the configuration file `/etc/dynamic-power.conf`.
This file is automatically created on the first run and will not be overwritten on subsequent runs.

### Example Configuration:

```ini
# dynamic-power.conf

# CPU load thresholds (in decimal values)
LOW_LOAD=1.0
HIGH_LOAD=2.0
CHECK_INTERVAL=10

# AC notification path and file
AC_PATH="/sys/class/power_supply/ADP0/online"

# Power management tool: powerprofilesctl or asusctl
POWER_TOOL="powerprofilesctl"

# Process to monitor for recording mode
RECORDING_PROCESS="obs"

# Process to monitor for video editing mode
VIDEO_EDIT_PROCESS="kdenlive"

# EPP policies for each mode
EPP_POWER_SAVER="power"
EPP_BALANCED="balance_performance"
EPP_PERFORMANCE="performance"
EPP_RECORDING="balance_power"
```

## Usage

### **Daemon Control**
Once installed, the `dynamic-power-daemon` service will automatically adjust the system's power mode based on CPU load. You can manually control the service using `systemctl`:

```sh
# Start the service
sudo systemctl start dynamic-power-daemon.service

# Stop the service
sudo systemctl stop dynamic-power-daemon.service

# Restart the service
sudo systemctl restart dynamic-power-daemon.service

# Check the status of the service
sudo systemctl status dynamic-power-daemon.service
```

### **Monitoring Utility**
A separate utility, `dynamic-power-monitor`, is available to track the daemon's parameters in real time. Run it in a terminal with:

```sh
dynamic-power-monitor
```

This utility will display:
- AC power status (Plugged In / On Battery)
- CPU load
- Active power profile
- EPP policy (if supported)
- Recording mode status

Press **'q'** or **Ctrl-C** to exit.

## Uninstallation

To uninstall the daemon:

1. Disable and stop the systemd service:

   ```sh
   sudo systemctl disable dynamic-power-daemon.service
   sudo systemctl stop dynamic-power-daemon.service
   ```

2. Remove the files:

   ```sh
   sudo rm /usr/local/bin/dynamic_power_daemon.sh
   sudo rm /usr/local/bin/dynamic_power_monitor.sh
   sudo rm /etc/dynamic-power.conf
   sudo rm /etc/systemd/system/dynamic-power-daemon.service
   ```

3. Optionally, remove the `bc` package if you no longer need it:

   ```sh
   sudo pacman -Rns bc
   ```

## License

This project is licensed under the GPL-3.0-or-later License.

