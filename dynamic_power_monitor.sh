#!/usr/bin/env bash
# Console Monitor for Dynamic-Power Daemon  — warns about config gaps
set -euo pipefail

########################################
#  0.  Load config & detect gaps       #
########################################
CONFIG_FILE="/etc/dynamic-power.conf"
# fall-back defaults (same list as daemon)
LOW_LOAD=1.0       HIGH_LOAD=2.0       CHECK_INTERVAL=1
AC_PATH="/sys/class/power_supply/ADP0/online"
POWER_TOOL="powerprofilesctl"
EPP_POWER_SAVER="power"  EPP_BALANCED="balance_performance"  EPP_PERFORMANCE="performance"
QUIET_PROCESSES="obs-studio"
RESPONSIVE_PROCESSES="kdenlive"
QUIET_EPP="balance_power"       RESPONSIVE_MIN_PROFILE="balanced"

[[ -f $CONFIG_FILE ]] && source "$CONFIG_FILE"

# gather missing keys for display
declare -a MISSING_VARS=()
ensure_var() {
    local n=$1; local def=$2
    if [[ -z "${!n:-}" ]]; then printf -v "$n" '%s' "$def"; MISSING_VARS+=("$n"); fi
}
ensure_var LOW_LOAD              "1.0"
ensure_var HIGH_LOAD             "2.0"
ensure_var AC_PATH               "/sys/class/power_supply/ADP0/online"
ensure_var POWER_TOOL            "powerprofilesctl"
ensure_var QUIET_PROCESSES       "obs-studio"
ensure_var RESPONSIVE_PROCESSES  "kdenlive"

########################################
#  1.  Helpers                          #
########################################
csv_to_array() { local IFS=','; read -ra "$1" <<<"$2"; }
is_any_running() { for p in "$@"; do pgrep -x "$p" &>/dev/null && return 0; done; return 1; }

csv_to_array QUIET_LIST      "$QUIET_PROCESSES"
csv_to_array RESPONSIVE_LIST "$RESPONSIVE_PROCESSES"

ACTIVE_PROFILE_CMD() {
    case "$POWER_TOOL" in
        powerprofilesctl) powerprofilesctl get ;;
        asusctl) asusctl profile -p | awk -F': *' '/Preset/ {print tolower($2)}' ;;
        *) echo "unknown" ;;
    esac
}

EPP_PATH="/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference"
EPP_SUPPORTED=false; [[ -f $EPP_PATH ]] && EPP_SUPPORTED=true

trap 'tput cnorm; clear; exit' SIGINT SIGTERM
tput civis

########################################
#  2.  UI loop                          #
########################################
while true; do
    clear
    echo "Dynamic-Power Daemon — Live Monitor"
    echo "=================================="

    # Missing keys display
    if (( ${#MISSING_VARS[@]} )); then
        echo -e "\e[33m⚠  Missing keys in $CONFIG_FILE: ${MISSING_VARS[*]}"
        echo "   Using safe defaults — please edit the file and restart.\e[0m"
        echo
    fi

    # AC status & load
    AC_STATUS=$([[ -f $AC_PATH && $(<"$AC_PATH") -eq 1 ]] && echo "Plugged-In" || echo "Battery")
    LOAD=$(awk '{print $1}' /proc/loadavg)
    PROFILE=$(ACTIVE_PROFILE_CMD)
    EPP=$($EPP_SUPPORTED && cat "$EPP_PATH" || echo "N/A")

    printf " AC Power      : %s\n" "$AC_STATUS"
    printf " CPU Load      : %s\n" "$LOAD"
    printf " Power Profile : %s\n" "$PROFILE"
    printf " EPP Policy    : %s\n" "$EPP"
    echo

    # Mode status
    QUIET=$([[ is_any_running "${QUIET_LIST[@]}" ]] && echo Active || echo Inactive)
    RESP=$([[ is_any_running "${RESPONSIVE_LIST[@]}" ]] && echo Active || echo Inactive)

    printf " Quiet mode       : %-8s (%s)\n" "$QUIET" "$QUIET_PROCESSES"
    printf " Responsive mode  : %-8s (%s)\n" "$RESP" "$RESPONSIVE_PROCESSES"

    echo -e "\nPress 'q' to quit."
    read -t 1 -n 1 k && [[ $k == q ]] && break
done

tput cnorm; clear