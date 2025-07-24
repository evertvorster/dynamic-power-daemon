
# Dynamic‑Power Project — Working Summary  
*(Generated 2025‑07‑22 18:48)*

## 1 · Repository Layout

```
dynamic-power.service               # root systemd unit (daemon)
python/
├── dynamic_power/
│   ├── __init__.py                 # root daemon
│   ├── cli.py                      # CLI helper
│   ├── config.py                   # config loader / upgrader
│   ├── dbus_interface.py           # daemon → user DBus
│   ├── dynamic_power_user.py       # per‑user helper (process matching)
│   ├── dynamic_power_command.py    # tray UI
│   ├── dynamic_power_session_helper.py  # user‑session supervisor (sensors)
│   └── …                           # epp.py, sensors.py, …
├── resources/
│   ├── dbus/org.dynamic_power.Daemon.conf
│   └── systemd-user/
│       ├── dynamic_power_session.service
│       ├── dynamic_power_command.service
│       └── 90-dynamic-power.preset
└── share/dynamic-power/dynamic-power.yaml   # template (version 6)
```

## 2 · Systemd Units

| Unit | Scope | Purpose |
|------|-------|---------|
| `dynamic-power.service` | **system** | Root daemon — power‑profile logic |

## 3 · DBus Interfaces

| Name | Bus | Methods / Signals |
|------|-----|-------------------|
| `org.dynamic_power.Daemon` | **system** | root → user (existing) |
| `org.dynamic_power.UserBus` | **session** | `Ping()`, `GetMetrics()`, `PowerStateChanged` |

## 4 · Roadmap & Progress

| Phase | Goal | Status |
|-------|------|--------|
| 0 | Session plumbing | **Done** |
| 1 | Sensor polling (`GetMetrics`) | **Done** |
| 2 | Tray reads DBus metrics | **Done** |
| 3 | Panel overdrive toggle | **Done** |
| 4 | Refresh‑rate discovery / switch | ❑ Not started |
| 5 | KDE panel auto‑hide | ❑ |
| 6 | GPU‑MUX notification | ❑ |
| 7 | Tray UI toggles | ❑ |
| 8 | Retire YAML runtime files | ❑ |
| 9 | Packaging polish | ❑ |

## 5 · Current Config Schema (v 6)

```yaml
version: 6

panel:
  overdrive:
    enable_on_ac: true        # overdrive 1 on AC, 0 on BAT
  # refresh_rate, auto_hide: to be added

# existing keys ...
```

## 6 · Known Issues / TODO

* **Tray import failure** on Arch `python-dbus-next 0.2.3`; needs:
  ```python
  from dbus_next.message_bus import MessageBus
  ```
* Remove YAML polling once tray fully relies on DBus.
* Implement refresh‑rate switching logic in session helper.
* Add user notifications for non‑Hybrid mux mode.

## 7 · Next Immediate Task

1. Patch `dynamic_power_command.py` import block to support old `dbus_next`:
   ```python
   try:
       from dbus_next.sync import MessageBus
   except ImportError:
       from dbus_next.message_bus import MessageBus
   ```
2. Confirm tray runs on Arch package `python-dbus-next 0.2.3`.

---


## 2025‑07‑23

* **Panel over‑drive**: Implemented via `asusctl armoury panel_overdrive` and now confirmed working – toggles automatically on AC ↔ battery events.
* **Desktop integration**: Added `dynamic-power.svg` icon and a `.desktop` file installed to `/usr/share/applications`, icon to `/usr/share/pixmaps`. KDE now lists *Dynamic Power* and users can add it to autostart easily.
* **Systemd user services deprecated**: Session helper is now started via autostart, launching `dynamic_power_user` and the tray UI internally. User‑session systemd units have been removed from packaging.
