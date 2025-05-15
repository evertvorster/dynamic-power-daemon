#!/usr/bin/env bash
# Dynamic-Power Daemon — auto-detect AC adapter + quiet/responsive modes
set -euo pipefail

CONFIG_FILE="/etc/dynamic-power.conf"

################################################
# 1. Generate config template if it’s missing  #
################################################
create_default_config() {
    mkdir -p "$(dirname "$CONFIG_FILE")"
    cat >"$CONFIG_FILE"<<'EOF'
#############################
#  Dynamic-Power Daemon v3  #
#  Auto-generated template  #
#############################

# ---------- Load thresholds ----------
LOW_LOAD=1.0
HIGH_LOAD=2.0
CHECK_INTERVAL=10          # seconds

# ---------- AC adapter path ----------
# Leave empty to let the daemon auto-detect a valid
#   /sys/class/power_supply/*/online   entry.
# Common examples (uncomment one):
# AC_PATH="/sys/class/power_supply/ADP0/online"
# AC_PATH="/sys/class/power_supply/AC/online"
AC_PATH=""

# ---------- Backend ----------
POWER_TOOL="powerprofilesctl"      # or "asusctl"

# ---------- EPP per standard profile ----------
EPP_POWER_SAVER="power"
EPP_BALANCED="balance_performance"
EPP_PERFORMANCE="performance"

# ---------- Special modes ----------
QUIET_PROCESSES="obs-studio"
RESPONSIVE_PROCESSES="kdenlive,steam"

QUIET_EPP="balance_power"
RESPONSIVE_MIN_PROFILE="balanced"
EOF
}

[[ -f $CONFIG_FILE ]] || create_default_config
# shellcheck source=/etc/dynamic-power.conf
source "$CONFIG_FILE"

################################################
# 2. Validate / auto-detect AC_PATH            #
################################################
auto_detect_ac_path() {
    local configured="$AC_PATH"
    local found=""
    local cand

    # If user specified a working path, keep it
    if [[ -n $configured && -f $configured ]]; then
        return
    fi

    # Search common names first
    local common=(ADP0 AC ACAD ACPI MENCHR)
    for name in "${common[@]}"; do
        cand="/sys/class/power_supply/$name/online"
        [[ -f $cand ]] && { found=$cand; break; }
    done

    # Fallback: first mains-type adapter we find
    if [[ -z $found ]]; then
        for cand in /sys/class/power_supply/*/online; do
            [[ -f $cand ]] || continue
            if grep -qiE "mains|ac" "${cand%/*}/type"; then
                found=$cand; break
            fi
        done
    fi

    if [[ -n $found ]]; then
        AC_PATH=$found
        logger -t dynamic_power "Configured AC_PATH invalid; using $AC_PATH"
    else
        logger -t dynamic_power "Cannot locate a valid AC adapter path; forcing power-saver when in doubt."
    fi
}
auto_detect_ac_path

################################################
# 3. Fill missing keys with defaults + warn    #
################################################
declare -A DEF=(
  [LOW_LOAD]=1.0  [HIGH_LOAD]=2.0  [CHECK_INTERVAL]=10
  [POWER_TOOL]="powerprofilesctl"
  [EPP_POWER_SAVER]="power"  [EPP_BALANCED]="balance_performance" [EPP_PERFORMANCE]="performance"
  [QUIET_PROCESSES]="obs-studio"  [RESPONSIVE_PROCESSES]="kdenlive"
  [QUIET_EPP]="balance_power"  [RESPONSIVE_MIN_PROFILE]="balanced"
)
MISSING=()
for k in "${!DEF[@]}"; do
    [[ -z "${!k:-}" ]] && { printf -v "$k" '%s' "${DEF[$k]}"; MISSING+=("$k"); }
done
(( ${#MISSING[@]} )) && logger -t dynamic_power "Config missing keys: ${MISSING[*]}"

################################################
# 4. Helpers                                   #
################################################
csv_to_array() { local IFS=','; read -ra "$1" <<<"$2"; }
is_any_running() { for p; do pgrep -x "$p" &>/dev/null && return 0; done; return 1; }

csv_to_array QUIET_LIST      "$QUIET_PROCESSES"
csv_to_array RESPONSIVE_LIST "$RESPONSIVE_PROCESSES"

set_power() {
    case $POWER_TOOL in
      powerprofilesctl) powerprofilesctl set "$1" ;;
      asusctl)
        case $1 in power-saver) asusctl profile quiet ;;
                 balanced) asusctl profile balanced ;;
              performance) asusctl profile performance ;; esac ;;
      *) logger -t dynamic_power "Unknown POWER_TOOL '$POWER_TOOL'"; exit 1;;
    esac
}
set_epp() {
    local val=$1
    [[ -z $val || ! -e /sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference ]] && return
    echo "$val" | tee /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference >/dev/null
}

################################################
# 5. Main loop                                 #
################################################
LAST=""
while true; do
    AC_ONLINE=$([[ -f $AC_PATH ]] && cat "$AC_PATH" || echo 0)

    QUIET_RUN=false ; RESP_RUN=false
    is_any_running "${QUIET_LIST[@]}"      && QUIET_RUN=true
    is_any_running "${RESPONSIVE_LIST[@]}" && RESP_RUN=true

    if $QUIET_RUN && [[ $AC_ONLINE == 1 ]]; then
        MODE="power-saver"; set_epp "$QUIET_EPP"
    else
        if [[ $AC_ONLINE == 1 ]]; then
            LOAD=$(awk '{print $1}' /proc/loadavg)
            if   (( $(echo "$LOAD < $LOW_LOAD" | bc -l) )); then MODE="power-saver"
            elif (( $(echo "$LOAD < $HIGH_LOAD"| bc -l) )); then MODE="balanced"
            else MODE="performance"; fi

            $RESP_RUN && [[ $MODE == power-saver ]] && MODE="$RESPONSIVE_MIN_PROFILE"
        else
            MODE="power-saver"
        fi
        case $MODE in
          power-saver)  set_epp "$EPP_POWER_SAVER" ;;
          balanced)     set_epp "$EPP_BALANCED"    ;;
          performance)  set_epp "$EPP_PERFORMANCE" ;;
        esac
    fi

    if [[ $MODE != $LAST ]]; then
        set_power "$MODE"
        logger -t dynamic_power "Switched to $MODE"
        LAST=$MODE
    fi

    sleep "$CHECK_INTERVAL"
done
