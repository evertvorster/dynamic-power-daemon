# DBus Communication – Dynamic Power Daemon

_Last updated: 2025-07-24_

This document describes how inter-process communication is handled using **DBus** between the system daemon, user-space helpers, and GUI tray.

---

## 🔧 DBus Services & Interfaces

| Component | Bus | DBus Name | Interface Class | Defined In |
|-----------|-----|-----------|------------------|-------------|
| **Daemon** (`dynamic_power`) | **system** | `org.dynamic_power.Daemon` | `DaemonInterface` | `dbus_interface.py` |
| **Session Helper** (`dynamic_power_session_helper`) | **session** | `org.dynamic_power.UserBus` | `UserBusIface` | `dynamic_power_session_helper.py` |

---

## 🧭 Message Flow Summary

### 1. `dynamic_power_command → dynamic_power` (via system bus)
- Triggered when user changes profile in the tray
- Method: `SetUserProfile(string profile)`

### 2. `dynamic_power_user → dynamic_power` (via system bus)
- Sends:
  - `SetProfile(profile, is_user_override=False)`
  - `SetLoadThresholds(low: float, high: float)`

### 3. `dynamic_power_session_helper → dynamic_power_user`
- Supervisor: spawns user + GUI processes, no DBus calls

### 4. `dynamic_power_user → dynamic_power_session_helper` (via session bus)
- Reads from `org.dynamic_power.UserBus`
- Calls:
  - `GetMetrics()`
  - Observes `PowerStateChanged(string)` signal

### 5. `dynamic_power_session_helper`
- Polls power supply, load, battery
- Emits signal: `PowerStateChanged(string)`
- Provides metrics as `{str: Variant}` via `GetMetrics()`

---

## 🧱 DBus Interface Details

### org.dynamic_power.Daemon

Defined in: `dbus_interface.py`

```python
class DaemonInterface(ServiceInterface):
    @method()
    def SetProfile(profile: 's', is_user_override: 'b') -> None

    @method()
    def SetUserProfile(profile: 's') -> None

    @method()
    def SetLoadThresholds(low: 'd', high: 'd') -> None
```

---

### org.dynamic_power.UserBus

Defined in: `dynamic_power_session_helper.py`

```python
class UserBusIface(ServiceInterface):
    @method()
    def GetMetrics() -> 'a{sv}'

    @signal()
    def PowerStateChanged(power_state: 's') -> None
```

All values in `GetMetrics()` are wrapped in `dbus_next.Variant`, e.g.:

```python
Variant('s', "AC"), Variant('d', 0.97)
```

---

## 🧪 Notes

- Uses `dbus_next` (0.2/0.3 compatible)
- Session helper owns `UserBus` and feeds metrics to GUI and user monitor.
- Tray and user monitor both send control commands to system daemon.
- All communication is async.

---