#!/usr/bin/env bash
set -euo pipefail

CONFIG_FILE="/etc/dynamic-power.conf"
[[ -f $CONFIG_FILE ]] && source "$CONFIG_FILE"

############################################
#  Detect missing / empty config entries   #
############################################
declare -A DEFAULTS=(
  [LOW_LOAD]=1.0
  [HIGH_LOAD]=2.0
  [CHECK_INTERVAL]=1
  [AC_PATH]="/sys/class/power_supply/ADP0/online"
  [POWER_TOOL]="powerprofilesctl"

  [EPP_POWER_SAVER]="power"
  [EPP_BALANCED]="balance_performance"
  [EPP_PERFORMANCE]="performance"

  [QUIET_PROCESSES]="obs-studio"
  [RESPONSIVE_PROCESSES]="kdenlive"

  [QUIET_EPP]="balance_power"
  [RESPONSIVE_MIN_PROFILE]="balanced"
)

MISSING=()

for key in "${!DEFAULTS[@]}"; do
  if [[ -z "${!key:-}" ]]; then          # not set or empty
    printf -v "$key" '%s' "${DEFAULTS[$key]}"
    MISSING+=("$key")
  fi
done

############################################
#  Helper functions                        #
############################################
csv_to_array() { local IFS=','; read -ra "$1" <<<"$2"; }
is_any_running() { for p; do pgrep -x "$p" &>/dev/null && return 0; done; return 1; }

csv_to_array QUIET_LIST      "$QUIET_PROCESSES"
csv_to_array RESPONSIVE_LIST "$RESPONSIVE_PROCESSES"

get_active_profile() {
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

############################################
#  Main loop                               #
############################################
while true; do
  clear
  echo "Dynamic-Power Daemon — Live Monitor"
  echo "=================================="

  if (( ${#MISSING[@]} )); then
    echo -e "\e[33m⚠  The following keys are missing from $CONFIG_FILE:"
    echo "   ${MISSING[*]}"
    echo -e "   Defaults are in use — edit the file and restart.\e[0m"
    echo
  fi

  AC_STATE=$([[ -f $AC_PATH && $(<"$AC_PATH") -eq 1 ]] && echo "Plugged-In" || echo "Battery")
  LOAD=$(awk '{print $1}' /proc/loadavg)
  PROFILE=$(get_active_profile)
  EPP=$($EPP_SUPPORTED && cat "$EPP_PATH" || echo "N/A")

  printf " AC Power      : %s\n" "$AC_STATE"
  printf " CPU Load      : %s\n" "$LOAD"
  printf " Power Profile : %s\n" "$PROFILE"
  printf " EPP Policy    : %s\n\n" "$EPP"

  QUIET=$(is_any_running "${QUIET_LIST[@]}"      && echo Active || echo Inactive)
  RESP=$(is_any_running "${RESPONSIVE_LIST[@]}" && echo Active || echo Inactive)

  printf " Quiet mode       : %-8s (%s)\n" "$QUIET" "$QUIET_PROCESSES"
  printf " Responsive mode  : %-8s (%s)\n" "$RESP" "$RESPONSIVE_PROCESSES"

  echo -e "\nPress 'q' to quit."
  read -t 1 -n 1 key && [[ $key == q ]] && break
done

tput cnorm; clear
