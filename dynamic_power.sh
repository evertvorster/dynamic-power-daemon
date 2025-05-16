#!/usr/bin/env bash
# Dynamic-Power Daemon  v3.1.2
# • Quiet / Responsive process lists
# • Smart AC-adapter auto-detect
# • Correct profile threshold logic
# • Safe EPP writes (no exit on busy)
# • Accurate powerprofilesctl support check
set -uo pipefail            # leave -e off so we can trap errors

CONFIG_FILE="/etc/dynamic-power.conf"

###############################################################################
# 1. create config template if missing                                        #
###############################################################################
if [[ ! -f $CONFIG_FILE ]]; then
  mkdir -p /etc
  cat >"$CONFIG_FILE"<<'EOF'
#############################
#  Dynamic-Power Daemon     #
#############################

LOW_LOAD=1.0
HIGH_LOAD=2.0
CHECK_INTERVAL=10

# Leave blank to auto-detect adapter
AC_PATH=""

POWER_TOOL="powerprofilesctl"          # or "asusctl"

QUIET_PROCESSES="obs-studio"
RESPONSIVE_PROCESSES="kdenlive,steam"

QUIET_EPP="balance_power"
RESPONSIVE_MIN_PROFILE="balanced"

EPP_POWER_SAVER="power"
EPP_BALANCED="balance_performance"
EPP_PERFORMANCE="performance"
EOF
fi

# shellcheck source=/etc/dynamic-power.conf
source "$CONFIG_FILE"

###############################################################################
# 2. fill defaults + warn if keys missing                                     #
###############################################################################
declare -A DEF=(
 [LOW_LOAD]=1.0  [HIGH_LOAD]=2.0  [CHECK_INTERVAL]=10
 [POWER_TOOL]="powerprofilesctl"
 [QUIET_PROCESSES]="obs-studio"   [RESPONSIVE_PROCESSES]="kdenlive"
 [QUIET_EPP]="balance_power"      [RESPONSIVE_MIN_PROFILE]="balanced"
 [EPP_POWER_SAVER]="power"        [EPP_BALANCED]="balance_performance" [EPP_PERFORMANCE]="performance"
)
GAPS=()
for k in "${!DEF[@]}"; do
  [[ -z "${!k:-}" ]] && { printf -v "$k" '%s' "${DEF[$k]}"; GAPS+=("$k"); }
done
(( ${#GAPS[@]} )) && logger -t dynamic_power "Config missing keys: ${GAPS[*]} (defaults applied)"

###############################################################################
# 3. auto-detect AC adapter path                                              #
###############################################################################
if [[ -z $AC_PATH || ! -f $AC_PATH ]]; then
  for cand in /sys/class/power_supply/{ADP0,AC,ACAD,ACPI,MENCHR,*/online}; do
    [[ -f $cand ]] || continue
    grep -qiE "mains|ac" "${cand%/*}/type" && { AC_PATH=$cand; break; }
  done
  logger -t dynamic_power "Using auto-detected AC_PATH=$AC_PATH"
fi
[[ ! -f $AC_PATH ]] && logger -t dynamic_power "No valid AC adapter; defaulting to power-saver when on battery."

###############################################################################
# 4. helpers                                                                  #
###############################################################################
command -v bc >/dev/null || { logger -t dynamic_power "bc utility missing — install bc"; exit 1; }

csv_to_array(){ local IFS=','; read -ra "$1" <<<"$2"; }
is_any_running(){ for p; do pgrep -x "$p" &>/dev/null && return 0; done; return 1; }

csv_to_array QLIST "$QUIET_PROCESSES"
csv_to_array RLIST "$RESPONSIVE_PROCESSES"

set_profile() {
  local tgt=$1 ok=1
  case $POWER_TOOL in
    powerprofilesctl)
      if powerprofilesctl list | grep -qwE "[[:space:]*]*${tgt}[:[:space:]]"; then
        powerprofilesctl set "$tgt"
      else
        ok=0
      fi
      ;;
    asusctl)
      case $tgt in
        power-saver) asusctl profile quiet ;;
        balanced)    asusctl profile balanced ;;
        performance) asusctl profile performance ;;
        *) ok=0 ;;
      esac ;;
    *) ok=0 ;;
  esac
  (( ok )) || logger -t dynamic_power "Profile $tgt unsupported by $POWER_TOOL"
}

set_epp() {
  local val=$1
  [[ -z $val ]] && return
  set +o pipefail                    # suppress tee errors
  for path in /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference; do
    [[ -w $path ]] && echo "$val" > "$path" 2>/dev/null
  done
  set -o pipefail
}

###############################################################################
# 5. main loop                                                                #
###############################################################################
LAST=""
while sleep "$CHECK_INTERVAL"; do
  AC_ON=$([[ -f $AC_PATH && $(<"$AC_PATH") -eq 1 ]] && echo 1 || echo 0)

  QUIET=false; RESP=false
  is_any_running "${QLIST[@]}" && QUIET=true
  is_any_running "${RLIST[@]}" && RESP=true

  if $QUIET && [[ $AC_ON == 1 ]]; then
    MODE=power-saver; set_epp "$QUIET_EPP"
  else
    if [[ $AC_ON == 1 ]]; then
      LOAD=$(awk '{print $1}' /proc/loadavg)
      if (( $(echo "$LOAD < $LOW_LOAD" | bc -l) )); then
        MODE=power-saver
      elif (( $(echo "$LOAD < $HIGH_LOAD" | bc -l) )); then
        MODE=balanced
      else
        MODE=performance
      fi
      $RESP && [[ $MODE == power-saver ]] && MODE="$RESPONSIVE_MIN_PROFILE"
    else
      MODE=power-saver
    fi

    case $MODE in
      power-saver)  set_epp "$EPP_POWER_SAVER" ;;
      balanced)     set_epp "$EPP_BALANCED"    ;;
      performance)  set_epp "$EPP_PERFORMANCE" ;;
    esac
  fi

  if [[ $MODE != $LAST ]]; then
    set_profile "$MODE"
    logger -t dynamic_power "Switched to $MODE (load=$(awk '{print $1}' /proc/loadavg))"
    LAST=$MODE
  fi
done
