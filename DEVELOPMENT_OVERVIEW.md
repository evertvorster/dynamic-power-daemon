# Dynamic‑Power‑Daemon – Development Overview & Roadmap  
*(Last updated: 2025‑07‑23)*

This document captures the **current state**, **design decisions**, and the **forward roadmap** for *dynamic‑power‑daemon* (DPD).  
Its chief purpose is to let future ChatGPT (or human) sessions get up‑to‑speed instantly and avoid re‑deriving context that already exists.

---

## 1. Project Snapshot

| Item | Status |
|------|--------|
| Language | Python ≥ 3.10 |
| Packaging | Arch Linux PKGBUILD (`dynamic‑power‑daemon` on AUR) |
| Install method | `make install` (root Makefile) + system/user systemd units |
| Primary components | `dynamic_power` (root daemon) · `dynamic_power_session_helper` · `dynamic_power_user` · `dynamic_power_command` (Qt tray) |
| IPC | DBus (`org.dynamic_power.Daemon`) + YAML state files in `/run` (to be phased out) |
| Config | YAML at `/etc/dynamic_power.yaml` + `~/.config/dynamic_power/config.yaml` |
| Latest tag | **v4.1.6** – “Systemd service integration” |
| Lead maintainer | Evert Vorster (GitHub @evertvorster) |

---

## 2. High‑Level Architecture

```mermaid
graph TD
  subgraph Root
    DPD[dynamic_power (root daemon)]
  end
  subgraph User Session
    DPU(dynamic_power_user) ---|DBus| DPD
    DPS(dynamic_power_session_helper)
    DPC(dynamic_power_command · Qt Tray)
  end
  DPU --- DPC
  DPS --- DPD
  file1[/etc/dynamic_power.yaml/] --> DPD
  file2[~/.config/dynamic_power/config.yaml] --> DPU
  state[/run/dynamic_power_state.yaml/] <--> {All}
```

* **DPD** – Runs as *root*; selects power profiles & EPP, exposes DBus API.  
* **DPS** – Wrapper that starts `dynamic_power_user`, handles sensors, overrides.  
* **DPU** – Monitors running processes & user overrides; sends hints to DPD.  
* **DPC** – Tray UI; graphs load, threshold sliders, mode buttons.

---

## 3. Completed Milestones (≤ v4.1.0)

| Date | Milestone |
|------|-----------|
| 2025‑07‑22 | Python rewrite reached feature parity with Bash version. |
| 2025‑07‑22 | Root Makefile moved to repo root; site‑packages install. |
| 2025‑07‑22 | System & user systemd units packaged and auto‑enabled. |
| 2025‑07‑23 | Tray UI start‑order fixed (`graphical-session.target`). |
| 2025‑07‑23 | AUR package updated (`pkgrel=1`). |

---

## 4. Forward Roadmap

| Phase | Feature | Key tasks | Priority |
|-------|---------|-----------|----------|
| **3** | Panel‑Overdrive Toggle (Asus) | Verify `asusctl armoury panel_overdrive` dry‑run; integrate switch into DPS; UI checkbox. | 🟢 *In Progress* |
| **4** | Refresh‑Rate Auto‑switch | Detect displays (`xrandr`/`kscreen`); add config (`display.min_hz`, `ac_hz`, …); DBus call; UI toggle. | 🟡 |
| **5** | KDE Panel Auto‑Hide | `qdbus org.kde.plasmashell` script; toggle based on AC/BAT; config flag. | 🟡 |
| **6** | Merge DPU into DPS | One user‑session binary; update service files; remove duplicated polling loop. | 🟠 |
| **7** | Replace YAML state with DBus signals | Add `MatchesChanged`, `ProfileChanged` signals; update Tray to listen. | 🟠 |
| **8** | GPU‑MUX & dGPU notifier | Read sysfs; notify on battery; optional tray badge. | 🟠 |
| **9** | UI Polish | Colour badges for matched processes; display power source; draggable threshold lines (complete). | 🟠 |
| **10** | CI & Tests | GitHub Actions: flake8, pytest, wheel build, `makepkg` in docker, artefacts. | 🟡 |

Legend: 🟢 = in progress · 🟡 = next · 🟠 = later

---

## 5. Key Design Decisions (Why?)

1. **Site‑packages install (option A)** – aligns with Arch Guidelines; multi‑python friendly.  
2. **Systemd units** – one *root* daemon, two *user* units; enables auto‑start and isolation.  
3. **DBus over sockets** – avoids root <-> user pipe hazards, language‑agnostic.  
4. **YAML config** – easy manual editing, version‑upgradeable (`config.py` handles schema bump).  
5. **Qt 6 + PyQtGraph** – minimal deps for plotting, fits KDE & GNOME environments.  
6. **Avoid `pip` during package build** – all deps must be system packages (`depends=()`).

---

## 6. Open Technical Debt

* **Hard‑coded paths** (`/run/dynamic_power_state.yaml`) – will go away when DBus signals land.  
* **EPP control dropped** – revisit if powerprofilesctl adds native EPP.  
* **Wayland vs X11 refresh handling** – provisional design in roadmap phase 4.  
* **Tests sparse** – only smoke tests; need unit coverage around process matcher and DBus.

---

## 7. Contribution & Branching

* **`main`** – stable; merge via PR after CI green.  
* **feature/xyz** – short‑lived branches; rebase often.  
* **release‑tags** – `vX.Y.Z`. AUR tracks the latest tag.

CI will run `flake8`, `pytest`, and build the PKGBUILD in a container.  
A failing test blocks merge to `main`.

---

## 8. Quick Reference for ChatGPT Sessions

> **If you’re ChatGPT:**  
> * Read this file first.  
> * Respect the roadmap table for next work item.  
> * Maintain Arch packaging constraints (no `pip install`).  
> * Use the root Makefile for installs with `DESTDIR`.  
> * Keep systemd unit names stable unless refactoring plan is approved.

---

## 9. Appendix – Current Systemd Units

```bash
# system daemon (root)
systemctl status dynamic_power.service

# user session helper (starts DPU)
systemctl --user status dynamic_power_session.service

# tray UI (Qt)
systemctl --user status dynamic_power_command.service
```

---

*Document generated by ChatGPT‑o3, 2025‑07‑23.*

