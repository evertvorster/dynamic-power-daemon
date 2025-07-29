# Battery-Specific Powersaving Features (Planned or In Progress)

This document outlines features discussed or partially implemented that are specifically intended to enhance power savings while the system is on battery. These include runtime behavior changes, hardware feature toggles, and user interface considerations.

---


## Extracts from `DBUS_COMMUNICATION.md`
- - Polls power supply, load, battery

## Extracts from `FEATURE_TOGGLES.md`
- This document outlines the architecture and implementation strategy for **feature toggles** controlling optional power-related behaviors when the system is running on battery power.
- kde_autohide_on_battery: false
- 3. **If power state is battery**, and `features.<name> == true`
- if features.get("kde_autohide_on_battery") and power == "BAT":
- - Simulate config change on battery/AC

## Extracts from `SCREEN_REFRESH_FEATURE.md`
- The goal of this feature is to **dynamically reduce the refresh rate of each display when running on battery** to save power, and **restore the maximum rate when plugged into AC power**.
- - Power source change (AC ↔ Battery)
- - On battery: set each to its known minimum
- When on battery power, reducing the screen refresh rate can save power. When on AC power, the maximum refresh rate should be used for the best user experience.
- - Positioned under the battery/AC power status line.
- - On Battery power: set **minimum** refresh rate.
- - Detect power state changes (AC/Battery).

## Extracts from `PANEL_OVERDRIVE.md`
- Preserve battery life by disabling the display’s *panel overdrive* when the
- laptop runs on battery, and re‑enable it on AC—*only if* the user requests it
- * Disable the display’s **panel overdrive** on battery, re‑enable on AC.

## Extracts from `dynamic_power_design_brief.md`
- - React to AC ↔ battery transitions.
- - Disable on battery.
- - Enable auto-hide on battery.
- - Lower refresh rate on battery (e.g., 60 Hz).
- - Read load average, CPU frequency, battery status.
- - [ ] **Implement sensor polling** (`load`, `cpu_freq`, `battery`).
- | Panel overdrive toggle on AC ↔ battery | ⏳ |
- | KDE panel auto-hide on battery | ⏳ |


---

## 🔧 Implementation Notes

From the current system architecture, we know the following:

- The daemon (`dynamic_power`) **detects power source status** via `sensors.py` and uses this to limit high-power profiles unless explicitly overridden.
- A "user override" flag is required to allow performance mode on battery.
- GUI (`dynamic_power_command`) displays battery state and power source, read from `/sys/class/power_supply/{battery_id}/status`.
- Planned improvements include:

### 1. **Panel Overdrive Handling**
- **Goal:** Disable panel overdrive when on battery to reduce screen power usage.
- **Control Location:** Centralized via `sensors.py` which should expose a `get_panel_overdrive_status(cfg)` function.
- **Implementation Notes:**
  - Status box already in GUI.
  - Uses `sysfs` or vendor-specific control files.
  - Controlled by config flag: `panel_overdrive: true|false`.

### 2. **KDE Panel Auto-hide Toggle**
- **Goal:** Enable KDE panel auto-hide on battery to avoid screen burn and reduce backlight usage.
- **Configurable:** Via YAML config.
- **Control Interface:** Likely requires interfacing with `qdbus` or KDE config files.
- **Fallback:** Do nothing if KDE not detected.

### 3. **Battery-aware Profile Restrictions**
- **Goal:** Disallow `performance` or `balanced` unless `is_user_override = True`.
- **Status:** Implemented in `dynamic_power` daemon as of July 2025.
- **Logic Path:** In DBus interface call handler (`SetProfile`), checks power source and denies high-power unless explicitly overridden.

### 4. **Session Helper Wake Lock Awareness (Planned)**
- **Idea:** Prevent relaunch of user/GUI agents if system is suspended and on battery.
- **Status:** Not started.
- **Optional Tools:** `upower`, `systemd-inhibit` insights.

### 5. **Battery Charge Threshold Monitoring (Future Idea)**
- **Idea:** Only allow high-power modes when battery is over a given % (e.g., >50%).
- **Status:** Not implemented.
- **Needs:** Add battery capacity polling to `sensors.py` and threshold config key.

