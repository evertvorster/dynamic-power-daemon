#!/bin/bash

CONFIG_FILE="/etc/dynamic-power.conf"
AC_PATH="/sys/class/power_supply/ADP0/online"
EPP_PATH="/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference"
MONITOR_PROCESS="obs"
POWER_TOOL="powerprofilesctl"

# Load configuration
if [[ -f "$CONFIG_FILE" ]]; then
    source "$CONFIG_FILE"
fi

# Check if EPP is supported
EPP_SUPPORTED=false
if [[ -f "$EPP_PATH" ]]; then
    EPP_SUPPORTED=true
fi

trap "tput cnorm; clear; exit" SIGINT SIGTERM

tput civis  # Hide cursor
while true; do
    clear
    echo "Dynamic Power Daemon Monitor"
    echo "============================"
    echo "Checking system parameters..."
    
    # AC power status
    if [[ -f "$AC_PATH" ]] && grep -q "1" "$AC_PATH"; then
        AC_STATUS="Plugged In"
    else
        AC_STATUS="On Battery"
    fi
    echo "AC Power: $AC_STATUS"
    
    # CPU load
    LOAD=$(awk '{print $1}' /proc/loadavg)
    echo "CPU Load: $LOAD"
    
    # Active power profile
    ACTIVE_PROFILE="$(powerprofilesctl get)"
    echo "Active Profile: $ACTIVE_PROFILE"
    
    # EPP policy
    if [[ "$EPP_SUPPORTED" == true ]]; then
        EPP_POLICY=$(cat "$EPP_PATH")
        echo "EPP Policy: $EPP_POLICY"
    else
        echo "EPP Policy: Not Supported"
    fi
    
    # Monitoring process
    if pgrep -x "$MONITOR_PROCESS" > /dev/null; then
        RECORDING_MODE="Active"
    else
        RECORDING_MODE="Inactive"
    fi
    echo "Recording Mode: $RECORDING_MODE"
    
    echo "\nPress 'q' to quit."
    read -t 1 -n 1 key
    if [[ "$key" == "q" ]]; then
        break
    fi

done

tput cnorm  # Show cursor again
clear
