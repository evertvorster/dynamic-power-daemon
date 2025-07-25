# 🧩 DESIGN_OVERVIEW.md — Architecture & Strategy

This document outlines the high-level architecture and guiding design philosophy of the `dynamic_power` project.

---

## 🧱 Architecture Summary

The system is split across **three main components**, each with clear responsibilities:

| Component                | Role                                         | Runs As         |
|--------------------------|----------------------------------------------|------------------|
| `dynamic_power` (daemon) | Applies system-level power changes via DBus | Root (systemd)   |
| `dynamic_power_user`     | Detects running processes, sends overrides  | User (systemd)   |
| `dynamic_power_command`  | GUI tray applet for monitoring and control  | User (Qt tray)   |

---

## 🔄 Inter-Component Communication

- All communication between user components and the daemon uses **DBus**.
- Runtime state and control overrides are shared via:
  - `/run/dynamic_power_state.yaml` ← written by daemon
  - `/run/user/{uid}/dynamic_power_control.yaml` ← user override
  - `/run/user/{uid}/dynamic_power_matches.yaml` ← matched processes

---

## 🛠 Design Philosophy

1. **Minimal Runtime Dependencies**
   - No pip-installed libraries — all dependencies available via system packages
   - Lightweight YAML and DBus libraries only

2. **Config-Driven Behavior**
   - System config: `/etc/dynamic_power.yaml`
   - User config: `~/.config/dynamic_power/config.yaml`
   - Fallback template: `/usr/share/dynamic-power/`

3. **Modular and Fail-Safe**
   - Each component fails gracefully and logs via systemd journal
   - GUI and user script do not require each other to function, but enhance functionality together

4. **Override Precedence Logic**
   1. User override (explicit via GUI) — highest priority
   2. Process match (automatic via user config)
   3. Daemon dynamic switching (based on load and AC/BAT state)

---

## 📐 Config Flow

- Config values are loaded once and cached, except:
  - `dynamic_power_user` watches for user config file changes via `inotify`
  - GUI reads config at launch and applies edits immediately

---

## 📦 Packaging and Runtime

- Installed via `make install` or AUR
- Python modules installed to site-packages
- Systemd services:
  - `dynamic_power.service` (root daemon)

---

This overview serves as a reference for anyone joining development or maintaining the project.
