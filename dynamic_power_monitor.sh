#!/usr/bin/env bash
# Live monitor for Dynamic-Power Daemon  v3.2.0  (override controls)
set -u

CONFIG_FILE="/etc/dynamic-power.conf"
OVERRIDE_FILE="/run/dynamic-power.override"

[[ -f $CONFIG_FILE ]] && source "$CONFIG_FILE"

# ---------- defaults & gap detection ----------
declare -A DEF=(
 [POWER_TOOL]="powerprofilesctl" [QUIET_PROCESSES]="obs-studio"
 [RESPONSIVE_PROCESSES]="kdenlive" [LOW_LOAD]=1.0 [HIGH_LOAD]=2.0
)
GAPS=(); for k in "${!DEF[@]}"; do [[ -z "${!k:-}" ]] && { printf -v "$k" '%s' "${DEF[$k]}"; GAPS+=("$k"); }; done

# ---------- AC path validation ----------
CONFIGURED="$AC_PATH"
if [[ -z $CONFIGURED || ! -f $CONFIGURED ]]; then
  for cand in /sys/class/power_supply/{ADP0,AC,ACAD,ACPI,MENCHR,*/online}; do
    [[ -f $cand ]] || continue
    grep -qiE "mains|ac" "${cand%/*}/type" && { AC_PATH=$cand; break; }
  done
else
  AC_PATH=$CONFIGURED
fi

# ---------- helpers ----------
csv_to_array(){ local IFS=','; read -ra "$1" <<<"$2"; }
is_any_running(){ for p; do pgrep -x "$p" &>/dev/null && return 0; done; return 1; }
get_prof(){ [[ $POWER_TOOL == powerprofilesctl ]] && powerprofilesctl get || asusctl profile -p|awk -F': *' '/Preset/{print tolower($2)}'; }
csv_to_array QLIST "$QUIET_PROCESSES"; csv_to_array RLIST "$RESPONSIVE_PROCESSES"
EPP_PATH="/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference"

trap 'tput cnorm; clear; exit' INT TERM
tput civis

while true; do
  clear
  echo "Dynamic-Power Daemon — Live Monitor"
  echo "=================================="

  (( ${#GAPS[@]} )) && echo -e "\e[33m⚠  Missing keys in $CONFIG_FILE: ${GAPS[*]} (defaults active)\e[0m\n"
  [[ -n $CONFIGURED && $CONFIGURED != $AC_PATH ]] && \
    echo -e "\e[33m⚠  Configured AC_PATH invalid → using $AC_PATH\e[0m\n"

  AC=$([[ -f $AC_PATH && $(<"$AC_PATH") -eq 1 ]] && echo "Plugged-In" || echo "Battery")
  LOAD=$(awk '{print $1}' /proc/loadavg)
  PROF=$(get_prof)
  EPP=$([[ -f $EPP_PATH ]] && cat "$EPP_PATH" || echo "N/A")

  printf " AC Power      : %s\n CPU Load      : %s\n Power Profile : %s\n EPP Policy    : %s\n\n" "$AC" "$LOAD" "$PROF" "$EPP"

  printf " Quiet mode    : %-8s (%s)\n" "$(is_any_running "${QLIST[@]}" && echo Active || echo Inactive)" "$QUIET_PROCESSES"
  printf " Responsive    : %-8s (%s)\n" "$(is_any_running "${RLIST[@]}" && echo Active || echo Inactive)" "$RESPONSIVE_PROCESSES"

  # -------- override status --------
  OV="dynamic"
  [[ -f $OVERRIDE_FILE ]] && OV=$(<"$OVERRIDE_FILE")
  [[ $OV != dynamic ]] && echo -e "\n\e[36mOverride active → $OV\e[0m"

  echo -e "\nKeys:  d-dynamic  b-balanced  s-power-saver  p-performance  q-quit"
  read -t 1 -n 1 key
  case $key in
    d) echo dynamic     > "$OVERRIDE_FILE" ;;
    b) echo balanced    > "$OVERRIDE_FILE" ;;
    s) echo power-saver > "$OVERRIDE_FILE" ;;
    p) echo performance > "$OVERRIDE_FILE" ;;
    q) break ;;
  esac
done

tput cnorm; clear
