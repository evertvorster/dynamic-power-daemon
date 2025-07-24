# Screen Refresh Rate Control – Feature Design

_Last updated: 2025-07-24_

This document outlines the design plan for the **screen refresh rate control** feature in Dynamic Power.

The goal of this feature is to **dynamically reduce the refresh rate of each display when running on battery** to save power, and **restore the maximum rate when plugged into AC power**.

---

## ✅ Summary

| Aspect | Description |
|--------|-------------|
| **Feature Name** | `screen_refresh` |
| **Config Location** | `~/.config/dynamic_power/config.yaml` under `features:` block |
| **Feature Toggle** | `features.screen_refresh: true` |
| **Default Behavior (even when off)** | Collect and display refresh rate info |
| **Activation** | When toggle is ON and power state changes |
| **UI Display** | `☑ Screen Hz:` followed by a list of `Monitor#: CurrentRate` entries |

---

## 🧩 UI Behavior

In the tray app (dynamic_power_command), display:

```
☑ Screen Hz: 0–60.00Hz, 1–144.00Hz
```

Where:
- Checkbox toggles the feature
- Monitor numbers map to logical display IDs
- Values are updated dynamically

---

## 🧠 Design Requirements

### 🟢 On startup:
- Detect all connected displays
- Determine for each:
  - Current refresh rate
  - Maximum and minimum refresh rates

### 🔄 On system state changes:
- Respond to:
  - Power source change (AC ↔ Battery)
  - Monitor connect/disconnect
  - Refresh rate change (if another app changed it)

---

## 🕵️ Data Sources and Strategies

| Data | Ideal Method | Fallback |
|------|--------------|----------|
| Connected displays | udev event or sysfs tree | `xrandr` (X11) |
| Refresh rate (current/min/max) | DRM via `/sys/class/drm/*/modes` | `xrandr`, `wayland-info` |
| Power state | Already tracked | – |

---

## 🔌 Power-Efficient Design

- Avoid polling
- Prefer event-driven updates via udev or inotify
- Only run external tools (like `xrandr`) if triggered by known events
- Cache monitor info and re-query only when topology changes

---

## 💡 Implementation Plan

### Phase 1: Display Info Gathering (Even When Feature OFF)
- At startup, collect all active monitors and their current refresh
- Store per-monitor:
  - `monitor_id`
  - `current_hz`
  - `min_hz`
  - `max_hz`
- Expose over session DBus as part of `GetMetrics()`

### Phase 2: UI Display
- Render row with toggle + per-monitor rates
- Disable checkbox when no monitors are found

### Phase 3: Apply Rate Change When Enabled
- On AC: set each screen to its known maximum
- On battery: set each to its known minimum
- Use `xrandr` or appropriate system API

---

## ❗ Challenges & Considerations

### Wayland vs X11
- `xrandr` is X11-only
- For Wayland:
  - May need per-compositor support (KDE, GNOME)
  - Possible use of `kwin` scripting or `org.kde.KWin` DBus APIs
  - Fallback: detect Wayland and disable feature with log note

### Headless or docking changes
- If monitor count changes, refresh internal list
- If running headless or display off, feature should safely no-op

### Daemon vs user space
- All refresh control logic should run in `dynamic_power_user.py`
- Only display values in the tray

---

## 🧪 Testing

- Plug/unplug AC and watch refresh rate change
- Add/remove external monitors
- Manually change resolution or refresh with external tool and verify re-sync

---

Second version, some a lot of things may be duplicated below here. It is to be considered an update of duplicates above.


# Screen Refresh Rate Control Feature

This document outlines the design and implementation considerations for a feature in `dynamic_power_user` that adjusts the screen refresh rate depending on the system's power state.

## Purpose

When on battery power, reducing the screen refresh rate can save power. When on AC power, the maximum refresh rate should be used for the best user experience.

## Configuration

This feature is controlled in the user's YAML configuration file:

```yaml
features:
  screen_refresh: true
```

Even when `screen_refresh` is set to `false`, the system should still gather information about connected monitors and their refresh rates for display purposes.

## UI Behavior

In the main `dynamic_power_command` interface:

- The feature appears as a toggle checkbox.
- Label: **"Screen Hz:"**
- The display format: `[Monitor ID] - [Current Refresh Rate]`
  - Example: `eDP-1 - 60Hz`
- Positioned under the battery/AC power status line.

## Information Required

For each connected monitor:

- **Monitor ID**
- **Current refresh rate**
- **Maximum refresh rate**
- **Minimum refresh rate**

## Behavior

- On AC power: set **maximum** refresh rate.
- On Battery power: set **minimum** refresh rate.
- When the feature is disabled: do not change refresh rate, but still report monitor and refresh rate info in UI.

## Detection Requirements

- Detect display connect/disconnect events.
- Detect refresh rate change events.
- Detect power state changes (AC/Battery).

## Polling Considerations

Polling should be avoided to minimize power consumption. Prefer:

- File watchers (`/sys/class/drm/`) if needed.
- **DBus signals** for real-time event updates.
- Polling only when unavoidable.

## Investigation Summary: Determining Screen Refresh Rates and Connected Displays

### X11-Compatible Options

- **`xrandr`**: CLI tool for querying/changing display settings.
  - Example: `xrandr --current`
  - Reliable for X11. Not supported under Wayland.

### Wayland-Compatible Options

- **`wlr-randr`**: For wlroots-based compositors.
- **`weston-info`**: Lists refresh rates and connected outputs (in mHz).
- **`kscreen-doctor`** (KDE):
  - Lists outputs and refresh rates.
  - Allows setting of refresh rates.
  - KDE-native and scriptable.
  - Example:
    ```
    Output 0: eDP-1 enabled connected priority 0 Panel
      Modes:
        1920x1080@60 (preferred)
        1920x1080@48
    ```

### KDE Integration

- KDE uses `KScreen2` with DBus support.
- `kscreen-doctor` is preferred:
  - Minimal overhead.
  - Integrates with Plasma.
  - Can be watched for output change events over DBus.

### Hotplug Detection

- Monitor `/sys/class/drm` for changes.
- Use KScreen DBus events (for KDE).
- Wayland compositors like Sway may have different APIs.

## Design Considerations

- Cache output info on startup.
- Use DBus to listen for:
  - Power state changes.
  - Screen configuration changes.
- Update refresh rate **only when needed**.
- Avoid external commands unless triggered by DBus or system event.

## Implementation Steps (to be defined later)

1. Detect available screens and their capabilities using `kscreen-doctor`.
2. Hook into the existing power state DBus signals.
3. Update the refresh rate only when power state or display configuration changes.
4. Show the info in the UI with minimal overhead.

## Future Enhancements

- Allow user to specify custom min/max refresh rates per monitor.
- Automatically detect preferred performance/power-saving modes.
- Integrate with display configuration UIs.
- Graceful fallback to X11 or sway-specific tools.

