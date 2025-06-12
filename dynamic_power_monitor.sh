#!/usr/bin/env bash
# Dynamic-Power Live Monitor  v3.2  (conditional full refresh)
set -u

CONFIG_FILE="/etc/dynamic-power.conf"
OVERRIDE_FILE="/run/dynamic-power.override"
[[ -f $CONFIG_FILE ]] && source "$CONFIG_FILE"

# --- defaults / gaps ---
declare -A DEF=(
  [POWER_TOOL]="powerprofilesctl"
  [QUIET_PROCESSES]="obs-studio"
  [RESPONSIVE_PROCESSES]="kdenlive"
  [LOW_LOAD]=1.0 [HIGH_LOAD]=2.0
)
GAPS=(); for k in "${!DEF[@]}"; do
  [[ -z "${!k:-}" ]] && { printf -v "$k" '%s' "${DEF[$k]}"; GAPS+=("$k"); }
done

# --- AC path sanity ---
CONFIGURED="$AC_PATH"
if [[ -z $CONFIGURED || ! -f $CONFIGURED ]]; then
  for cand in /sys/class/power_supply/{ADP0,AC,ACAD,ACPI,MENCHR,*/online}; do
    [[ -f $cand ]] || continue
    grep -qiE "mains|ac" "${cand%/*}/type" && { AC_PATH=$cand; break; }
  done
else
  AC_PATH=$CONFIGURED
fi

# --- helpers ---
csv_to_array(){ local IFS=','; read -ra "$1" <<<"$2"; }
is_any_running(){ for p; do pgrep -x "$p" &>/dev/null && return 0; done; return 1; }
get_prof(){ [[ $POWER_TOOL == powerprofilesctl ]] && powerprofilesctl get \
           || asusctl profile -p | awk -F': *' '/Preset/{print tolower($2)}'; }
csv_to_array QLIST "$QUIET_PROCESSES"; csv_to_array RLIST "$RESPONSIVE_PROCESSES"
EPP_PATH="/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference"

trap 'tput cnorm; clear; exit' INT TERM
tput civis

REFRESH=1   # first loop should clear screen

while true; do
  # -------- full clear only when requested --------
  if (( REFRESH )); then
    clear
    REFRESH=0
  fi

  # -------- gather live data --------
  AC=$([[ -f $AC_PATH && $(<"$AC_PATH") -eq 1 ]] && echo "Plugged-In" || echo "Battery")
  LOAD=$(awk '{print $1}' /proc/loadavg)
  PROF=$(get_prof)
  EPP=$([[ -f $EPP_PATH ]] && cat "$EPP_PATH" || echo "N/A")
  QUIET=$(is_any_running "${QLIST[@]}" && echo Active || echo Inactive)
  RESP=$(is_any_running "${RLIST[@]}" && echo Active || echo Inactive)
  OVERRIDE=$(<"$OVERRIDE_FILE")

  # -------- build screen --------
  buffer=$(
cat <<EOF
Dynamic-Power Daemon — Live Monitor
==================================

 AC Power      : $AC
 CPU Load      : $LOAD
 Power Profile : $PROF
 EPP Policy    : $EPP

 Quiet mode    : $(printf '%-8s' "$QUIET") ($QUIET_PROCESSES)
 Responsive    : $(printf '%-8s' "$RESP") ($RESPONSIVE_PROCESSES)
EOF
)

  [[ ${#GAPS[@]} -gt 0 ]] && buffer+=$'\n\n\e[33m⚠ Missing keys: '"${GAPS[*]}"' (defaults active)\e[0m'
  [[ -n $CONFIGURED && $CONFIGURED != $AC_PATH ]] && \
      buffer+=$'\n\n\e[33m⚠ Configured AC_PATH invalid → using '"$AC_PATH"$'\e[0m'
  [[ $OVERRIDE != dynamic ]] && buffer+=$'\n\n\e[36mOverride active → '"$OVERRIDE"$'\e[0m'

  buffer+=$'\n\nKeys:\n  d - dynamic (auto)\n  s - power-saver\n  b - balanced\n  p - performance\n  q - quit'

  # -------- paint (fast path) --------
  tput cup 0 0
  printf "%s" "$buffer"
  tput cup "$(($(tput lines)-1))" "$(($(tput cols)-1))"
  printf "\033[0J"

  # -------- key handler --------
  if read -t 1 -n 1 k; then
    case $k in
      d) echo dynamic     > "$OVERRIDE_FILE"; REFRESH=1; continue ;;
      s) echo power-saver > "$OVERRIDE_FILE"; REFRESH=1; continue ;;
      b) echo balanced    > "$OVERRIDE_FILE"; REFRESH=1; continue ;;
      p) echo performance > "$OVERRIDE_FILE"; REFRESH=1; continue ;;
      q) break ;;
    esac
  fi
done

tput cnorm; clear
