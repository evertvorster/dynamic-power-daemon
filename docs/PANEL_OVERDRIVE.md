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

## Config file location and format

