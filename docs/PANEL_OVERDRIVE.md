# Panel Overdrive Auto‑Toggle

**Purpose**  
Preserve battery life by disabling the display’s *panel overdrive* when the
laptop runs on battery, and re‑enable it on AC—*only if* the user requests it
**and** the hardware supports it.

## Behaviour

| Condition | Action |
|-----------|--------|
| `panel.overdrive.enable_on_ac` **false** | Leave panel state untouched. |
| Setting **true** & **AC** power | Run `asusctl armoury panel_overdrive 1`. |
| Setting **true** & **BAT** power | Run `asusctl armoury panel_overdrive 0`. |

The helper detects power‑source flips in real time and sends the command only
when a change is necessary.

## Support detection

We call:

```bash
asusctl armoury --help
```

and look for the token `panel_overdrive:` in its output.  
If absent, `self._overdrive_supported` is set to **False** and no attempt is
made to toggle the panel.

## DBus exposure

* `GetMetrics()` now includes `panel_overdrive_enabled` (`bool`) so the tray
  can show the current state later.
* Signal `PanelOverdriveChanged(int enabled)` fires after every successful
  toggle.

## Implementation location

All logic lives in `dynamic_power_session_helper.py`; the root daemon is not
involved.

## Summary

* Disable the display’s **panel overdrive** on battery, re‑enable on AC.
* Gated by **user preference** at  
  `~/.config/dynamic_power/config.yaml ➜ features.panel_overdrive`.
* Skips all action when the machine’s firmware doesn’t expose `panel_overdrive`
  in `asusctl armoury --help`.
* Emits `panel_overdrive_enabled` in `GetMetrics()` and
  `PanelOverdriveChanged(int)` DBus signal so the tray can:
  * **display** the current state (future work)
  * **toggle** the feature on/off (future work)

## Future UI goals

1. Show a checkbox or indicator for “Panel overdrive active”.
2. Allow the user to toggle **features.panel_overdrive** directly from the tray
   (writing back to `config.yaml`).

   # Panel Overdrive Auto‑Toggle

## User preference

* Set in **`~/.config/dynamic_power/config.yaml`**  
  ```yaml
  features:
    panel_overdrive: true   # manage panel overdrive on AC
  ```
* If the key is **false** (or missing), the helper leaves the panel state
  untouched.

*Legacy compatibility*: the old key `panel.overdrive.enable_on_ac` in
`/etc/dynamic-power.yaml` is still honoured as a fallback so existing
installations keep working.

## Behaviour

| Power source | `features.panel_overdrive` | Action |
|--------------|---------------------------|--------|
| AC   | `true`  | `asusctl armoury panel_overdrive 1` |
| BAT  | `true`  | `asusctl armoury panel_overdrive 0` |
| *any*| `false` | **No change** |

The helper also checks `asusctl armoury --help` for `panel_overdrive:`; if
the token is missing, the feature is silently skipped.

## DBus exposure (for future UI)

* **Metric** `panel_overdrive_enabled` (bool) added to `GetMetrics()`.
* **Signal** `PanelOverdriveChanged(int enabled)` emitted on every change.
* The tray can later display the current state and provide a checkbox to
  flip `features.panel_overdrive` at runtime.

---



### 2025‑07‑23

The helper now calls `asusctl armoury panel_overdrive 1|0` directly whenever the power source flips.  
Tests show immediate switching with no race conditions when session helper is started via autostart (not systemd‑user).

