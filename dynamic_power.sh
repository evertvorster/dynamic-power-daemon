#!/bin/bash

# Load configuration
CONFIG_FILE="/etc/dynamic-power.conf"
LOW_LOAD=1.0
HIGH_LOAD=2.0
CHECK_INTERVAL=10
AC_PATH="/sys/class/power_supply/ADP0/online"
LAST_MODE=""

# Check if the configuration file exists, and create if not.
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Configuration file not found. Creating default configuration..."

    # Create the configuration file with default settings
    cat <<EOF > "$CONFIG_FILE"
# dynamic-power.conf

# CPU load thresholds (in decimal values)
LOW_LOAD=1.0
HIGH_LOAD=2.0
CHECK_INTERVAL=10
#AC notification path and file
AC_PATH="/sys/class/power_supply/ADP0/online"
EOF

    echo "Default configuration created at $CONFIG_FILE"
fi

# Read config if it exists
if [[ -f "$CONFIG_FILE" ]]; then
    source "$CONFIG_FILE"
fi

while true; do
    if [[ -f "$AC_PATH" ]] && grep -q "1" "$AC_PATH"; then
        LOAD=$(awk '{print $1}' /proc/loadavg)

        if (( $(echo "$LOAD < $LOW_LOAD" | bc -l) )); then
            NEW_MODE="power-saver"
        elif (( $(echo "$LOAD >= $LOW_LOAD" | bc -l) && $(echo "$LOAD < $HIGH_LOAD" | bc -l) )); then
            NEW_MODE="balanced"
        else
            NEW_MODE="performance"
        fi
    else
        NEW_MODE="power-saver"
    fi

    if [[ "$NEW_MODE" != "$LAST_MODE" ]]; then
        powerprofilesctl set "$NEW_MODE"
        echo "Switched to $NEW_MODE mode"
        LAST_MODE="$NEW_MODE"
    fi

    sleep $CHECK_INTERVAL
done
