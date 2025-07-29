# Dynamic Power Daemon

A modular, dynamic Linux power management system for desktops and laptops. Optimizes CPU profiles and EPP settings based on system load, power source, and user activity.

## ⚡ Features

- Auto-adjusts CPU power profiles dynamically
- Custom user configuration per app/process
- Manual override from system tray GUI
- Threshold-based tuning
- Battery-aware behavior
- DBus integration
- Modular YAML configuration

## 🧱 Architecture

- `dynamic_power`: Root daemon, applies system settings
- `dynamic_power_user`: Per-user agent for process matching
- `dynamic_power_command`: GUI controller and monitor
- `dynamic_power_session_helper`: Session initializer
- Configuration lives in `/etc/` and `~/.config/`
- State passed via DBus (YAML fallback to be phased out)

## 🧪 Getting Started

```bash
git clone https://github.com/evertvorster/dynamic-power-daemon.git
cd dynamic-power-daemon
sudo make install
```

## 🧠 Philosophy

- Minimal intrusion on the user experience
- Clear separation of config vs runtime
- YAML for static config only — runtime data moves to DBus
- Modular, extensible, and observable

## 📃 License

MIT License

## 📬 Contributing

Issues and PRs welcome. Please read the development guidelines in `docs/`.

---

> "Because your laptop should be smart enough to know when it's working too hard."
