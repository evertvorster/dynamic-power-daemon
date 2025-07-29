# Startup Sequence and Runtime Diagram

## Textual Overview

1. **System Boot / User Login**
2. `dynamic_power_session_helper` starts (via autostart).
3. Checks if `dynamic_power_user` and `dynamic_power_command` are running.
4. Launches both if missing.
5. `dynamic_power_user` begins monitoring user config and running processes.
6. `dynamic_power_command` shows tray icon and listens for user interaction.
7. `dynamic_power` daemon runs as a systemd service (root) and listens on DBus.

## Process Communication

```
[session_helper]──spawn──▶[user]─────DBus─────▶[daemon]
                            │                   ▲
                            ▼                   │
                         [GUI]──────writes─────▶(override YAML) [temp, to be replaced]
```
