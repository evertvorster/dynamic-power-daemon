#!/bin/bash

# Load configuration
CONFIG_FILE="/etc/dynamic-power.conf"
LOW_LOAD=1.0
HIGH_LOAD=2.0
CHECK_INTERVAL=10
AC_PATH="/sys/class/power_supply/ADP0/online"
POWER_TOOL="powerprofilesctl"  # Default power tool
LAST_MODE=""
RECORDING_PROCESS="obs-studio"
EPP_POWER_SAVER="power"
EPP_BALANCED="balance_performance"
EPP_PERFORMANCE="performance"
EPP_RECORDING="balance_power"
EPP_SUPPORTED=false

# Check if the configuration file exists, and create it if not.
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Configuration file not found. Creating default configuration..."

    cat <<EOF > "$CONFIG_FILE"
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
RECORDING_PROCESS="obs-studio"

# EPP policies for each mode
EPP_POWER_SAVER="power"
EPP_BALANCED="balance_performance"
EPP_PERFORMANCE="performance"
EPP_RECORDING="balance_power"
EOF

    echo "Default configuration created at $CONFIG_FILE"
fi

# Read config if it exists
if [[ -f "$CONFIG_FILE" ]]; then
    source "$CONFIG_FILE"
fi

# Check if AC_PATH exists, exit if not
if [[ ! -f "$AC_PATH" ]]; then
    echo "Error: AC status file '$AC_PATH' not found. Please check your configuration file at '$CONFIG_FILE'." >&2
    exit 1
fi

# Check if EPP is supported
if [[ -d /sys/devices/system/cpu/cpu0/cpufreq ]] && \
   [[ -f /sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference ]]; then
    EPP_SUPPORTED=true
    echo "EPP support detected. Enabling EPP policy management."
else
    echo "EPP not supported on this system. Disabling EPP policy management."
fi

set_power_mode() {
    local mode=$1
    if [[ "$POWER_TOOL" == "powerprofilesctl" ]]; then
        powerprofilesctl set "$mode"
    elif [[ "$POWER_TOOL" == "asusctl" ]]; then
        case "$mode" in
            power-saver) asusctl profile quiet ;;
            balanced) asusctl profile balanced ;;
            performance) asusctl profile performance ;;
        esac
    else
        echo "Unsupported power tool: $POWER_TOOL" >&2
        exit 1
    fi
}

set_epp_policy() {
    local policy=$1
    if [[ "$EPP_SUPPORTED" == true ]]; then
        echo "$policy" | tee /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference > /dev/null
    fi
}

while true; do
    if pgrep -x "$RECORDING_PROCESS" > /dev/null && grep -q "1" "$AC_PATH"; then
        NEW_MODE="power-saver"
        if [[ "$EPP_SUPPORTED" == true ]]; then
            set_epp_policy "$EPP_RECORDING"
        fi
        echo "Recording mode activated: Staying in power-saver mode with EPP set to $EPP_RECORDING."
    else
        if grep -q "1" "$AC_PATH"; then
            LOAD=$(awk '{print $1}' /proc/loadavg)

            if (( $(echo "$LOAD < $LOW_LOAD" | bc -l) )); then
                NEW_MODE="power-saver"
                if [[ "$EPP_SUPPORTED" == true ]]; then
                    set_epp_policy "$EPP_POWER_SAVER"
                fi
            elif (( $(echo "$LOAD >= $LOW_LOAD" | bc -l) && $(echo "$LOAD < $HIGH_LOAD" | bc -l) )); then
                NEW_MODE="balanced"
                if [[ "$EPP_SUPPORTED" == true ]]; then
                    set_epp_policy "$EPP_BALANCED"
                fi
            else
                NEW_MODE="performance"
                if [[ "$EPP_SUPPORTED" == true ]]; then
                    set_epp_policy "$EPP_PERFORMANCE"
                fi
            fi
        else
            NEW_MODE="power-saver"
            if [[ "$EPP_SUPPORTED" == true ]]; then
                set_epp_policy "$EPP_POWER_SAVER"
            fi
        fi
    fi

    if [[ "$NEW_MODE" != "$LAST_MODE" ]]; then
        set_power_mode "$NEW_MODE"
        echo "Switched to $NEW_MODE mode using $POWER_TOOL"
        if [[ "$EPP_SUPPORTED" == true ]]; then
            echo "EPP set to $(cat /sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference)"
        fi
        LAST_MODE="$NEW_MODE"
    fi

    sleep $CHECK_INTERVAL
done
