
import os
from .debug import debug_log

_valid_epp_modes = None
_current_epp = None

def detect_supported_modes():
    global _valid_epp_modes
    path = "/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_available_preferences"
    try:
        with open(path, "r") as f:
            content = f.read().strip()
            modes = content.split()
            debug_log("epp", f"Raw content of EPP file on cpu0: '{content}'")
            debug_log("epp", f"Parsed EPP modes: {modes}")
            _valid_epp_modes = modes
    except FileNotFoundError:
        print("[epp] Warning: EPP not supported on this system")
        _valid_epp_modes = []
    return _valid_epp_modes

def set_epp(epp_value):
    global _valid_epp_modes, _current_epp

    if _valid_epp_modes is None:
        detect_supported_modes()

    if not _valid_epp_modes:
        debug_log("epp", "No EPP support detected on this system.")
        return

    if epp_value not in _valid_epp_modes:
        debug_log("epp", f"'{epp_value}' is not a valid EPP mode on this system. Skipping.")
        return

    if epp_value == _current_epp:
        return  # Already set, skip

    success = 0
    fail = 0

    cpu_base = "/sys/devices/system/cpu"
    for entry in os.listdir(cpu_base):
        if not entry.startswith("cpu") or not entry[3:].isdigit():
            continue

        path = os.path.join(cpu_base, entry, "cpufreq", "energy_performance_preference")
        if not os.path.isfile(path):
            continue

        try:
            with open(path, "w") as f:
                f.write(epp_value)
            success += 1
        except Exception as e:
            debug_log("epp", f"Failed to set EPP on {path}: {e}")
            fail += 1

    if success > 0:
        _current_epp = epp_value
        print(f"[epp] EPP set to '{epp_value}' on {success} CPUs ({fail} failed)")
