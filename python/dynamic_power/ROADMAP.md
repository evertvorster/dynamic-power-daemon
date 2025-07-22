
# Dynamic‑Power Daemon – Roadmap & Progress  
_Last updated: 2025‑07‑22_

| Phase | Goal | Key Work Items | Status | Notes |
|-------|------|---------------|--------|-------|
| **0** | **Plumbing** – user‑session systemd unit, DBus stub, tray autostart | `dynamic_power_session.service`, `UserBus` skeleton | **Done** |
| **1** | **Sensor polling + DBus metrics** | CPU load, power‑supply, battery %, `GetMetrics()` + `PowerStateChanged` | **Done** |
| **2** | **Tray → DBus bridge** | Tray reads metrics over DBus (YAML fallback) | **Done** |
| **3** | **Panel over‑drive toggle** | Config key `panel.overdrive.enable_on_ac`, helper invokes `asusctl armoury panel_overdrive 1/0` on AC change | **⚙️ In progress** |
| **4** | **Refresh‑rate switching** | Discover min/max/current via `xrandr`, expose DBus API, apply on power change | Not started |
| **5** | **KDE panel auto‑hide** | Script Plasma DBus to auto‑hide on battery | Pending |
| **6** | **MUX‑mode notification** | Detect dGPU activity, suggest Hybrid | Pending |
| **7** | **Tray UI toggles** | Add check‑boxes / indicators | Pending |
| **8** | **Retire YAML runtime files** | Tray fully DBus‑driven | Pending |
| **9** | **Packaging & polish** | PKGBUILD (AUR), docs, CI | Pending |

## Recent Changes
* **2025‑07‑22**
  * Added `Config.get_panel_overdrive_config()` with sane default.
  * Session helper now toggles panel over‑drive via `asusctl` when power source changes.
  * Flexible import shim supports Arch’s `python-dbus-next 0.2.3`.

## Testing Phase 3

```bash
systemctl --user restart dynamic_power_session.service
journalctl --user -fu dynamic_power_session.service
```

Expect log lines:

```
… Sensor loop started
… Power source changed AC → BAT
… Running asusctl armoury panel_overdrive 0
… panel_overdrive set to 0
… Power source changed BAT → AC
… Running asusctl armoury panel_overdrive 1
… panel_overdrive set to 1
```

_© 2025 Dynamic‑Power Project_
