#!/usr/bin/env bash
# Dynamic-Power Daemon  — quiet / responsive aware + config-gap detection
set -euo pipefail

########################################
#  0.  Auto-generate config if absent  #
########################################
CONFIG_FILE="/etc/dynamic-power.conf"

create_default_config() {
    mkdir -p "$(dirname "$CONFIG_FILE")"
    cat >"$CONFIG_FILE"<<'EOF'
#############################
#  Dynamic-Power Daemon v2  #
#  Auto-generated template  #
#############################

########  General settings  ########
LOW_LOAD=1.0
HIGH_LOAD=2.0
CHECK_INTERVAL=10
AC_PATH="/sys/class/power_supply/ADP0/online"
POWER_TOOL="powerprofilesctl"   # or "asusctl"

########  EPP per profile ##########
EPP_POWER_SAVER="power"
EPP_BALANCED="balance_performance"
EPP_PERFORMANCE="performance"

########  Special modes  ###########
QUIET_PROCESSES="obs-studio,screenrec"
RESPONSIVE_PROCESSES="kdenlive,shotcut,resolve,steam"
QUIET_EPP="balance_power"
RESPONSIVE_MIN_PROFILE="balanced"
EOF
    echo "[dynamic_power] Default configuration created at $CONFIG_FILE"
}

[[ -f $CONFIG_FILE ]] || create_default_config
# shellcheck source=/etc/dynamic-power.conf
source "$CONFIG_FILE"

########################################
#  1.  Detect missing keys / defaults  #
########################################
declare -a MISSING_VARS=()

ensure_var() {
    local name=$1 default=$2
    if [[ -z "${!name:-}" ]]; then
        printf -v "$name" '%s' "$default"
        MISSING_VARS+=("$name")
    fi
}

# Critical keys with their internal defaults
ensure_var LOW_LOAD              "1.0"
ensure_var HIGH_LOAD             "2.0"
ensure_var CHECK_INTERVAL        "10"
ensure_var AC_PATH               "/sys/class/power_supply/ADP0/online"
ensure_var POWER_TOOL            "powerprofilesctl"
ensure_var EPP_POWER_SAVER       "power"
ensure_var EPP_BALANCED          "balance_performance"
ensure_var EPP_PERFORMANCE       "performance"
ensure_var QUIET_PROCESSES       "obs-studio"
ensure_var RESPONSIVE_PROCESSES  "kdenlive"
ensure_var QUIET_EPP             "balance_power"
ensure_var RESPONSIVE_MIN_PROFILE "balanced"

# Log once if anything was missing
if (( ${#MISSING_VARS[@]} )); then
    logger -t dynamic_power \
      "Config missing keys: ${MISSING_VARS[*]}. Using built-in defaults. Update $CONFIG_FILE."
fi

########################################
#  2.  Helpers                         #
########################################
csv_to_array() { local IFS=','; read -ra "$1" <<<"$2"; }
is_any_running() { for n in "$@"; do pgrep -x "$n" &>/dev/null && return 0; done; return 1; }

set_power_mode() {
    case "$POWER_TOOL" in
        powerprofilesctl) powerprofilesctl set "$1" ;;
        asusctl)
            case "$1" in
                power-saver) asusctl profile quiet ;;
                balanced)    asusctl profile balanced ;;
                performance) asusctl profile performance ;;
            esac ;;
        *) logger -t dynamic_power "Unknown POWER_TOOL '$POWER_TOOL'"; exit 1 ;;
    esac
}

set_epp() {
    [[ -z $1 || ! -e /sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference ]] && return
    echo "$1" | tee /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference >/dev/null
}

# Arrays for process checks
csv_to_array QUIET_LIST      "$QUIET_PROCESSES"
csv_to_array RESPONSIVE_LIST "$RESPONSIVE_PROCESSES"

########################################
#  3.  Main loop                       #
########################################
LAST_MODE=""
while true; do
    AC_ONLINE=$(<"$AC_PATH")
    QUIET_RUN=false
    RESP_RUN=false
    is_any_running "${QUIET_LIST[@]}"      && QUIET_RUN=true
    is_any_running "${RESPONSIVE_LIST[@]}" && RESP_RUN=true

    if $QUIET_RUN && [[ $AC_ONLINE == 1 ]]; then
        MODE="power-saver"; set_epp "$QUIET_EPP"
    else
        if [[ $AC_ONLINE == 1 ]]; then
            LOAD=$(awk '{print $1}' /proc/loadavg)
            if   (( $(echo "$LOAD < $LOW_LOAD"  | bc -l) )); then MODE="power-saver"
            elif (( $(echo "$LOAD < $HIGH_LOAD" | bc -l) )); then MODE="balanced"
            else MODE="performance"; fi
            # responsive floor
            if $RESP_RUN && [[ $MODE == "power-saver" ]]; then
                MODE="$RESPONSIVE_MIN_PROFILE"
            fi
        else
            MODE="power-saver"
        fi
        # default EPP per profile
        case $MODE in
            power-saver)  set_epp "$EPP_POWER_SAVER" ;;
            balanced)     set_epp "$EPP_BALANCED"    ;;
            performance)  set_epp "$EPP_PERFORMANCE" ;;
        esac
    fi

    if [[ $MODE != $LAST_MODE ]]; then
        set_power_mode "$MODE"
        logger -t dynamic_power "Switched to $MODE"
        LAST_MODE="$MODE"
    fi
    sleep "$CHECK_INTERVAL"
done
