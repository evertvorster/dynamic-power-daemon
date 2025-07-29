# Screen Refresh Rate Management (Planned Feature)

This document describes a planned feature to dynamically adjust the screen's refresh rate based on power source or system load, with the goal of conserving power on battery and enabling high refresh rate only when needed.

---

## 📚 References from Documentation


### `SCREEN_REFRESH_FEATURE.md`
- # Screen Refresh Rate Control – Feature Design
- This document outlines the design plan for the **screen refresh rate control** feature in Dynamic Power.
- The goal of this feature is to **dynamically reduce the refresh rate of each display when running on battery** to save power, and **restore the maximum rate when plugged into AC power**.
- | **Default Behavior (even when off)** | Collect and display refresh rate info |
- | **UI Display** | `☑ Screen Hz:` followed by a list of `Monitor#: CurrentRate` entries |
- ☑ Screen Hz: 0–60.00Hz, 1–144.00Hz
- - Current refresh rate
- - Maximum and minimum refresh rates
- - Refresh rate change (if another app changed it)
- | Refresh rate (current/min/max) | DRM via `/sys/class/drm/*/modes` | `xrandr`, `wayland-info` |
- - `current_hz`
- - `min_hz`
- - `max_hz`
- - Plug/unplug AC and watch refresh rate change
- # Screen Refresh Rate Control Feature
- This document outlines the design and implementation considerations for a feature in `dynamic_power_user` that adjusts the screen refresh rate depending on the system's power state.
- When on battery power, reducing the screen refresh rate can save power. When on AC power, the maximum refresh rate should be used for the best user experience.
- Even when `screen_refresh` is set to `false`, the system should still gather information about connected monitors and their refresh rates for display purposes.
- - Label: **"Screen Hz:"**
- - The display format: `[Monitor ID] - [Current Refresh Rate]`
- - Example: `eDP-1 - 60Hz`
- - **Current refresh rate**
- - **Maximum refresh rate**
- - **Minimum refresh rate**
- - On AC power: set **maximum** refresh rate.
- - On Battery power: set **minimum** refresh rate.
- - When the feature is disabled: do not change refresh rate, but still report monitor and refresh rate info in UI.
- - Detect refresh rate change events.
- ## Investigation Summary: Determining Screen Refresh Rates and Connected Displays
- - **`weston-info`**: Lists refresh rates and connected outputs (in mHz).
- - Lists outputs and refresh rates.
- - Allows setting of refresh rates.
- - Update refresh rate **only when needed**.
- 3. Update the refresh rate only when power state or display configuration changes.
- - Allow user to specify custom min/max refresh rates per monitor.

### `dynamic_power_design_brief.md`
- The project aims to expand the functionality of `dynamic_power`, a power profile manager for Linux, by introducing a modular user-space component that manages visual and power-saving tweaks (panel overdrive, refresh rate, auto-hide, notifications), handles sensors, and provides a clean interface (via DBus) for communication between helper utilities and the GUI.
- 3. **Display Refresh Rate**
- - Lower refresh rate on battery (e.g., 60 Hz).
- - Higher refresh rate on AC (e.g., 165 Hz).
- - Detect all connected outputs and their min/max/current refresh rates.
- - [ ] **Add refresh rate switching** logic.
- - [ ] **Query and store min/max/cur refresh rates per display.**
- - [ ] Add slider or dropdown for refresh rate preference.
- | Refresh rate min/max discovery | ⏳ |


---

## 🛠️ Planned Implementation Details

### Feature Goal
Reduce screen refresh rate to a power-saving value (e.g., 60Hz) when on battery or in low-load conditions, and restore to high refresh (e.g., 120Hz+) when on AC or when performance mode is enabled.

### Implementation Location
- Centralized via `sensors.py`
- Controlled by a feature toggle in `dynamic_power.yaml` (e.g., `screen_refresh_control: true`)
- `sensors.py` will expose:
  ```python
  def get_screen_refresh_status(cfg=None) -> str
  def set_screen_refresh_rate(rate: int)
  ```

### Control Mechanism
- Use `xrandr`, `wayland`, or `vendor-specific tools` to change refresh rates dynamically.
- Identify connected display(s) and supported modes.

### Triggers
- On power source change (battery <-> AC)
- On mode change (e.g., user selects performance mode)
- On resume from suspend

### Future Expansion
- GUI display of current refresh rate
- User-selectable refresh rate modes per profile
- Schedule-based control (e.g., restrict 144Hz during certain hours)

