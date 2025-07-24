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