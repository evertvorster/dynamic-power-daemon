# Known Technical Debt and Cleanup Areas

## ❌ Direct Config Access
- Some modules or test scripts directly read YAML files instead of using `config.py`.
- Goal: **All config reads go through `config.py`**.

## ❌ YAML-based Runtime State
- `/run/dynamic_power_state.yaml`, `/run/user/…` files still used for passing state.
- Goal: **Eliminate all YAML-based runtime signaling** and move to DBus.

## ⚠️ Missing Error Handling
- Some fallback behavior is not well defined if DBus is unavailable.
- GUI may silently fail if state file is missing.

## 🔁 Code Duplication
- Some similar logic duplicated across `user`, `GUI`, and `helper` scripts.

## 🚧 To Be Refactored
- Move all hardware logic to `sensors.py`.
- Ensure profile logic in `power_profiles.py` is consistently used by all modules.
