#!/usr/bin/env bash
set -euo pipefail

CONFIG_FILE="/etc/dynamic-power.conf"
[[ -f $CONFIG_FILE ]] && source "$CONFIG_FILE"

# ---------------- Default map -----------------
declare -A DEF=(
 [LOW_LOAD]=1.0 [HIGH_LOAD]=2.0 [CHECK_INTERVAL]=1
 [POWER_TOOL]="powerprofilesctl"
 [EPP_POWER_SAVER]="power" [EPP_BALANCED]="balance_performance" [EPP_PERFORMANCE]="performance"
 [QUIET_PROCESSES]="obs-studio" [RESPONSIVE_PROCESSES]="kdenlive"
 [QUIET_EPP]="balance_power" [RESPONSIVE_MIN_PROFILE]="balanced"
)
MISSING=()
for k in "${!DEF[@]}"; do
 [[ -z "${!k:-}" ]] && { printf -v "$k" '%s' "${DEF[$k]}"; MISSING+=("$k"); }
done

# ------------- AC path validation -------------
CONFIG_AC_PATH="${AC_PATH:-}"
detected_path=""
if [[ -n $CONFIG_AC_PATH && -f $CONFIG_AC_PATH ]]; then
  AC_PATH=$CONFIG_AC_PATH
else
  for cand in /sys/class/power_supply/*/online; do
    [[ -f $cand ]] || continue
    if grep -qiE "mains|ac" "${cand%/*}/type"; then detected_path=$cand; break; fi
  done
  [[ -n $detected_path ]] && AC_PATH=$detected_path
fi

# ----------- Helpers ------------
csv_to_array(){ local IFS=','; read -ra "$1" <<<"$2"; }
is_any_running(){ for p; do pgrep -x "$p" &>/dev/null && return 0; done; return 1; }
get_profile(){
  [[ $POWER_TOOL == powerprofilesctl ]] && powerprofilesctl get || \
  asusctl profile -p | awk -F': *' '/Preset/ {print tolower($2)}'
}
csv_to_array QLIST "$QUIET_PROCESSES"
csv_to_array RLIST "$RESPONSIVE_PROCESSES"
EPP_PATH="/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference"
EPP_OK=false; [[ -f $EPP_PATH ]] && EPP_OK=true
trap 'tput cnorm; clear; exit' SIGINT SIGTERM
tput civis

while true; do
 clear
 echo "Dynamic-Power Daemon — Live Monitor"
 echo "=================================="

 if (( ${#MISSING[@]} )); then
   echo -e "\e[33m⚠  Missing keys in $CONFIG_FILE: ${MISSING[*]}"
   echo -e "   Defaults active — edit the file & restart.\e[0m\n"
 fi

 if [[ -n $CONFIG_AC_PATH && $CONFIG_AC_PATH != $AC_PATH ]]; then
   echo -e "\e[33m⚠  Configured AC_PATH invalid → using $AC_PATH\e[0m\n"
 fi

 AC_STATE=$([[ -f $AC_PATH && $(<"$AC_PATH") -eq 1 ]] && echo "Plugged-In" || echo "Battery")
 LOAD=$(awk '{print $1}' /proc/loadavg)
 PROFILE=$(get_profile)
 EPP=$($EPP_OK && cat "$EPP_PATH" || echo "N/A")

 printf " AC Power      : %s\n" "$AC_STATE"
 printf " CPU Load      : %s\n" "$LOAD"
 printf " Power Profile : %s\n" "$PROFILE"
 printf " EPP Policy    : %s\n\n" "$EPP"

 QSTAT=$(is_any_running "${QLIST[@]}" && echo Active || echo Inactive)
 RSTAT=$(is_any_running "${RLIST[@]}" && echo Active || echo Inactive)

 printf " Quiet mode       : %-8s (%s)\n" "$QSTAT" "$QUIET_PROCESSES"
 printf " Responsive mode  : %-8s (%s)\n" "$RSTAT" "$RESPONSIVE_PROCESSES"

 echo -e "\nPress 'q' to quit."
 read -t 1 -n 1 k && [[ $k == q ]] && break
done

tput cnorm; clear
