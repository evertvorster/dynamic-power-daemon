# Dynamic Power Daemon

This is a lightweight Linux daemon that dynamically adjusts the system's power profile based on CPU load.

## Features
- Switches between `power-saver`, `balanced`, and `performance` modes dynamically based on CPU load.
- Always stays in `power-saver` mode when on battery power.
- Uses `/sys/class/power_supply/ADP0/online` to detect whether the system is plugged into AC power.
- Configurable thresholds via `/etc/dynamic-power.conf` (configuration file).

## Installation

### 1. Install Dependencies
Ensure that you have the required dependencies installed. This daemon requires `bc` for floating-point calculations. Install it using:

sudo pacman -S bc  # For Arch Linux users

### 2. Copy the Script
Copy the `dynamic_power.sh` script to `/usr/local/bin/`:

sudo cp dynamic_power.sh /usr/local/bin/
sudo chmod +x /usr/local/bin/dynamic_power.sh

### 3. Install the Configuration File
Copy the configuration file to `/etc/`:

sudo cp dynamic-power.conf /etc/

### 4. Install the Systemd Service (Optional)
To have the daemon start automatically, install the systemd service file:

sudo cp dynamic-power-daemon.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable dynamic-power-daemon.service
sudo systemctl start dynamic-power-daemon.service

This will set up the service to run at startup and ensure the daemon is running.

## Configuration

You can adjust the load thresholds and power profiles by editing the configuration file `/etc/dynamic-power.conf`.

### Example configuration:

# dynamic-power.conf

# CPU load thresholds (in decimal values)
low_threshold=1.0
medium_threshold=2.0
high_threshold=3.0

## Usage

Once installed, the `dynamic-power-daemon` service will automatically adjust the system's power mode based on CPU load. You can manually control the service using `systemctl`:

# Start the service
sudo systemctl start dynamic-power-daemon.service

# Stop the service
sudo systemctl stop dynamic-power-daemon.service

# Restart the service
sudo systemctl restart dynamic-power-daemon.service

# Check the status of the service
sudo systemctl status dynamic-power-daemon.service

## Uninstallation

To uninstall the daemon:

1. Disable and stop the systemd service:

   sudo systemctl disable dynamic-power-daemon.service
   sudo systemctl stop dynamic-power-daemon.service

2. Remove the files:

   sudo rm /usr/local/bin/dynamic_power.sh
   sudo rm /etc/dynamic-power.conf
   sudo rm /etc/systemd/system/dynamic-power-daemon.service

3. Optionally, remove the `bc` package if you no longer need it:

   sudo pacman -Rns bc

## License

This project is licensed under the GPL-3.0 License.

