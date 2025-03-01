# Dynamic Power Daemon

This is a lightweight Linux daemon that dynamically adjusts the system's power profile based on CPU load.

## Features
- Switches between `power-saver`, `balanced`, and `performance` modes dynamically.
- Always stays in `power-saver` mode on battery.
- Uses `/sys/class/power_supply/ADP0/online` to detect power state.
- Configurable thresholds via `/etc/dynamic-power.conf`.

## Installation
1. Copy `dynamic_power.sh` to `/usr/local/bin/`:
   ```bash
   sudo cp dynamic_power.sh /usr/local/bin/
   sudo chmod +x /usr/local/bin/dynamic_power.sh
