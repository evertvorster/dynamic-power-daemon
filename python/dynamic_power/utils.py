
import psutil
from .debug import debug_log, info_log, error_log

def get_process_override(cfg):
    overrides = cfg.get("process_overrides", [])
    matched = []

    for proc in psutil.process_iter(attrs=["name"]):
        pname = proc.info["name"]
        for override in overrides:
            if pname == override.get("process_name"):
                debug_log("utils", f"Matched process: {pname}")
                matched.append((override.get("priority", 0), override))

    if matched:
        matched.sort(reverse=True)
        chosen = matched[0][1]
        info_log("utils", f"Applying override: {chosen}")
        return chosen

    debug_log("utils", "No process overrides matched.")
    return None
