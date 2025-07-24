# Feature Toggles for Power-Sensitive Behavior

_Last updated: 2025-07-24_

This document outlines the architecture and implementation strategy for **feature toggles** controlling optional power-related behaviors when the system is running on battery power.

---

## 🔧 Where Configuration Lives

Feature toggles are stored in the **user config file**:

```yaml
features:
  kde_autohide_on_battery: false
  panel_overdrive: true
```

Located at:  
`~/.config/dynamic_power/config.yaml`

This file is:
- Actively monitored by `dynamic_power_user.py`
- Automatically reloaded when changed (via `getmtime()`)
- Edited by the tray UI (`dynamic_power_command.py`) using PyYAML and rewritten cleanly

---

## 🧠 Implementation Ownership

| Feature Trigger | Controlled By | DBus Method Used | File |
|-----------------|----------------|------------------|------|
| Power state changes (AC/BAT) | `dynamic_power_user.py` | None (internal state) | `dynamic_power_user.py` |
| UI toggle rendering | `dynamic_power_command.py` | Reads config & status from session bus | `dynamic_power_command.py` |
| Feature-specific logic | Pluggable inside user utility | Optional: status → daemon via DBus | `dynamic_power_user.py` |

---

## 🧩 Feature Rendering in UI

Each feature shows up as a **row** in the UI **below the Power Source label**.

Example (for `panel_overdrive`):

```
☑  Panel Overdrive      [AC]  → Enabled
```

Each line contains:
- A checkbox (toggle)
- Short label
- Live status (via DBus or internal polling)

The UI modifies `features:<key>` when the checkbox is changed.  

---

## 📜 Code Flow – Toggling a Feature

1. **User clicks checkbox in tray**
   → `dynamic_power_command.py` updates `config.yaml`

2. **`dynamic_power_user.py` detects file change**
   → Reloads config using `getmtime()` + `yaml.safe_load()`

3. **If power state is battery**, and `features.<name> == true`
   → Calls feature logic (e.g. `set_kde_autohide(True)`)

4. **If power state is AC**, or feature is disabled
   → Ensures feature is turned off (idempotent call or noop)

---

## 🔄 Power State Handling

- `dynamic_power_user.py` tracks power state transitions.
- When `power_source` flips, it evaluates which features are active.
- For each enabled feature, it executes the relevant logic:

```python
if features.get("kde_autohide_on_battery") and power == "BAT":
    enable_kde_autohide()
```

---

## 🔎 Where to Look (Code Locations)

| Purpose | File | Function / Section |
|--------|------|---------------------|
| Reload config on change | `dynamic_power_user.py` | `reload_if_changed()` or main loop |
| Trigger logic per feature | `dynamic_power_user.py` | inside power source check |
| Show toggle in UI | `dynamic_power_command.py` | `build_feature_row()` (future), layout section under power state |
| Update config file | `dynamic_power_command.py` | `save_user_config()` |

---

## 🧱 UI Behavior Requirements

- Each feature is shown as a toggle box + status
- Disabling a toggle **removes all effects immediately**
- UI saves only the `features` block when changed

---

## 🧪 Testing Recommendations

- Simulate config change on battery/AC
- Verify DBus reflects correct power state
- Check toggled feature applies or reverts correctly

---