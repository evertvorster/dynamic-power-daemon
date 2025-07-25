# 🚧 TASK_SCOPE.md — Active Development Scope

This document describes the current active feature being developed for the `dynamic_power` project, including rationale, goals, and in-scope boundaries.

---

## 🎯 Current Task: Finalize GUI-Integrated Control for Power Features

### 📌 Rationale
To provide the user with intuitive and centralized control over power management features, including:
- Dynamic vs manual power mode selection
- Visual feedback on system state (load, power source, panel overdrive)
- Persistent configuration options stored safely in the user config
- Transient system status delivered cleanly via DBus

### ✅ Goals
- Display current `features.panel_overdrive` setting in the GUI
- Allow the user to toggle this setting via a checkbox
- Store the setting directly in `~/.config/dynamic_power/config.yaml` under `features.panel_overdrive`
- GUI reads and writes this setting directly from the config
- DBus is not involved for this setting

### 🖼 UI Placement and Layout
- A box should be added directly **below the Battery Status display**
- Inside this box:
  - A **checkbox** appears on the **left**
  - Followed by the label: **"Panel Overdrive :"**
  - Followed by current status text: **"On"** or **"Off"** (based on config value)

### 📦 Scope
- `dynamic_power_command.py` (Qt GUI)
- `~/.config/dynamic_power/config.yaml` (read/write `features.panel_overdrive`)
- `/etc/dynamic_power.yaml` (read-only if needed)
- No changes to `dynamic_power_user.py`, daemon, or DBus

### ⚠ Considerations
- GUI may read and write configuration YAML files directly
- GUI must not write any transient status to YAML
- Transient information like power source, load, and active profile are handled via DBus
- Changes must comply with `CARDINAL_DEVELOPMENT_RULES.md`

---

This task is considered complete when:
- The GUI shows the live value of `features.panel_overdrive` from the user config
- Toggling the checkbox updates the value in the YAML file directly
- The GUI layout matches the specified format and placement
- No unintended parts of the codebase are modified
- A full git history is included in the project archive
