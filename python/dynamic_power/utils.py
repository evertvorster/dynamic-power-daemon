import psutil

def get_uptime_seconds():
    with open("/proc/uptime", "r") as f:
        return float(f.readline().split()[0])

def get_process_override(config):
    overrides = config.get("process_overrides", [])
    matches = []

    for override in overrides:
        name = override.get("process_name")
        if name:
            for proc in psutil.process_iter(attrs=["name"]):
                if proc.info["name"] == name:
                    matches.append(override)
                    break

    if not matches:
        return None

    return sorted(matches, key=lambda o: o.get("priority", 0), reverse=True)[0]
