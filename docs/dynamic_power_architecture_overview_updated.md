# Dynamic Power Daemon – Project Architecture & Data Flow Overview

## 📌 Project Goal

The `dynamic_power` project dynamically adjusts power profiles on Linux based on system load, power source, and running user applications. The project supports real-time overrides, GUI-based controls, and configurable logic for various hardware features. A major architectural goal is to shift from passing live state via YAML files toward using DBus for runtime coordination.

---

## ⚙️ High-Level Component Overview

- **`dynamic_power`** (daemon) – Root process managing actual profile switching and DBus.
- **`dynamic_power_user`** – User agent for detecting running applications and sending DBus overrides.
- **`dynamic_power_command`** – GUI tray app for live control and feedback.
- **`dynamic_power_session_helper`** – Autostart entry that launches the above and sets the environment.
- **YAML config files** – Only for static configuration; no runtime data should be stored here.

---

## 🐞 Debugging Architecture (New)

- The debug flag is set only in `dynamic_power_session_helper.py` using an **environment variable**:  
  `DYNAMIC_POWER_DEBUG=1`
- This variable is inherited by all subprocesses.
- `config.py` reads this variable at module init and exposes:
  ```python
  def is_debug_enabled() -> bool
  ```
- All `print()` and debug `LOG.debug()` calls across the project must use this flag.
- Important system logs (e.g., power source change, daemon start) use `LOG.info()` regardless of debug mode.

---

## 🧩 Responsibilities by File

### ✅ `config.py`
- Now also owns the runtime `debug` flag sourced from the environment.
- All other modules must consult `config.is_debug_enabled()` to emit debug output.

...

(unchanged sections follow)
