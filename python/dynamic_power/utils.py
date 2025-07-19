import psutil
from .debug import debug_log

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
        debug_log("utils", f"Chosen override: {chosen}")
        return chosen

    return None
