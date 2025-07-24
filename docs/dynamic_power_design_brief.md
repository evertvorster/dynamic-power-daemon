# 🧾 Design Brief: dynamic_power – User-Space Power Management Extensions

## Overview
The project aims to expand the functionality of `dynamic_power`, a power profile manager for Linux, by introducing a modular user-space component that manages visual and power-saving tweaks (panel overdrive, refresh rate, auto-hide, notifications), handles sensors, and provides a clean interface (via DBus) for communication between helper utilities and the GUI.

---

## ✅ Project Requirements

### 🔌 System Behavior
- React to AC ↔ battery transitions.
- Use a YAML-based config file stored in `/etc/dynamic_power.yaml`.
- All changes must follow privilege best practices (e.g., overdrive needs no root anymore).

### 🧠 Core Functionalities
1. **Panel Overdrive**
   - Enable on AC power.
   - Disable on battery.
   - Controlled via `asusctl armoury panel_overdrive <0|1>`.

2. **KDE Panel Auto-Hide**
   - Enable auto-hide on battery.
   - Disable it on AC.
   - Implemented via `qdbus` script evaluated by KDE PlasmaShell.

3. **Display Refresh Rate**
   - Lower refresh rate on battery (e.g., 60 Hz).
   - Higher refresh rate on AC (e.g., 165 Hz).
   - Implemented using `xrandr` (X11) or `kscreen-doctor` (Wayland).

4. **Sensor Readings**
   - Read load average, CPU frequency, battery status.
   - Write to a shared file or publish via DBus.

5. **Display Mode Discovery**
   - Detect all connected outputs and their min/max/current refresh rates.

6. **Notifications**
   - Send user notifications (e.g., recommend Hybrid GPU mode).
   - Use `org.freedesktop.Notifications` via DBus.

---

## 🛠️ Design Decisions

| Decision | Rationale |
|---|---|
| **Use `dynamic_power_session_helper`** | Consolidates logic into a single helper under systemd user unit. |
| **Run as user service (`systemd --user`)** | Ensures helpers run only after login, inherits user environment. |
| **All display tweaks run as user** | KDE and display control requires `$DISPLAY` and D-Bus session. |
| **DBus for communication** | Cleaner IPC with event-driven updates, replacing YAML polling. |
| **Retire root-only panel_overdrive** | New asusctl version supports user-space toggling. |
| **Initial step: create session service with stub** | Minimizes risk by validating the architecture before adding logic. |
| **Makefile installs systemd unit and preset** | Automates setup via Arch PKGBUILD. |

---

## 🗺️ Roadmap

### ✅ Phase 1: Bootstrap
- [x] Create `dynamic_power_session_helper.py` stub.
- [x] Add DBus interface (`Ping()` method).
- [x] Add `dynamic_power_session.service` (user unit).
- [x] Add `90-dynamic-power.preset` to auto-enable user unit.
- [x] Update Makefile to install new components.

### ⏳ Phase 2: Implement Core Features
- [ ] **Move dynamic_power_user logic** into the session helper.
- [ ] **Implement sensor polling** (`load`, `cpu_freq`, `battery`).
- [ ] **Add refresh rate switching** logic.
- [ ] **Add KDE panel auto-hide logic.**
- [ ] **Query and store min/max/cur refresh rates per display.**
- [ ] **Send GPU mode notification if in wrong mux mode.**

### 🚧 Phase 3: UI Integration
- [ ] Replace YAML polling in `dynamic_power_command` with DBus `GetMetrics()`.
- [ ] Display real-time updates from DBus signals.
- [ ] Add checkbox in UI for auto-hide toggle.
- [ ] Add slider or dropdown for refresh rate preference.

---

## 📋 Feature Checklist

| Feature | Status |
|---|---|
| Systemd user unit installed on login | ✅ |
| User helper owns `org.dynamic_power.UserBus` | ✅ |
| DBus method `Ping()` works | ✅ |
| Session helper restarts on failure | ✅ |
| Panel overdrive toggle on AC ↔ battery | ⏳ |
| KDE panel auto-hide on battery | ⏳ |
| Display refresh-rate switching | ⏳ |
| Sensor polling & metrics file | ⏳ |
| Refresh rate min/max discovery | ⏳ |
| KDE notification for GPU mux mode | ⏳ |
| Move from YAML polling to DBus in GUI | ⏳ |
| Makefile installs all files correctly | ✅ |

---

## 🧩 Modular Responsibilities

| Component | Responsibilities |
|---|---|
| `dynamic_power` (daemon) | Controls CPU/EPP, reads main config, triggers panel_od_helper if needed. |
| `dynamic_power_session_helper` | Hosts user-space DBus interface; runs session-wide logic. |
| `UserLogic` (merged from `dynamic_power_user`) | Process matching, user overrides. |
| `SensorLogic` (new) | Reads system metrics, triggers panel tweaks. |
| `dbus_user_interface.py` | Exposes `GetMetrics()` and signals. |
| `dynamic_power_command` | UI for users; connects to DBus for live updates. |

---

## 📂 Directory & File Layout (after Phase 1)

```
python/
├── dynamic_power/
│   ├── __init__.py
│   ├── cli.py
│   ├── config.py
│   ├── debug.py
│   ├── dynamic_power_command.py
│   ├── dynamic_power_user.py
│   ├── dynamic_power_session_helper.py   # NEW
│   ├── epp.py
│   ├── power_profiles.py
│   ├── sensors.py
│   └── dbus_interface.py
├── Makefile
├── resources/
│   ├── dbus/
│   │   └── org.dynamic_power.Daemon.conf
│   ├── systemd-user/
│   │   ├── dynamic_power_session.service   # NEW
│   │   └── 90-dynamic-power.preset         # NEW
├── share/
│   └── dynamic-power/
│       ├── dynamic-power.yaml
│       └── dynamic-power-user.yaml
```