# Debug Cleanup Plan for Dynamic Power Project

This document provides all the context and requirements for a new session tasked with modernizing the debug logging system across all Python scripts in the project.

---

## ✅ Objective

**Convert all existing debug output statements to use a centralized, runtime-controlled debug flag**.

This cleanup will ensure that:

- Only relevant output appears unless debugging is explicitly enabled.
- All modules follow a consistent pattern.
- The `config.py` module is the sole authority for determining debug state.

---

## 🔍 Where the Debug Flag Comes From

1. The debug flag is set **only at runtime** by the startup script `dynamic_power_session_helper.py`.
2. This is done via the **`DYNAMIC_POWER_DEBUG` environment variable**.
3. All subprocesses (e.g., `dynamic_power_user.py`, `dynamic_power_command.py`) inherit this environment variable when launched.
4. `config.py` reads this variable on import and provides a helper function for all modules to query it.

---

## 🧱 Implementation Details

### In `config.py`:
- Add this code:
  ```python
  import os
  DEBUG = os.getenv("DYNAMIC_POWER_DEBUG") == "1"

  def is_debug_enabled():
      return DEBUG
  ```

### In all other scripts:
- Replace raw `print(...)` and `LOG.debug(...)` statements with:
  ```python
  from dynamic_power import config

  if config.is_debug_enabled():
      print("...")  # Or log.debug(...)
  ```

### Logging vs Print:
- Favor `logging.debug()` for new code unless explicitly printing to console.
- System-level events (e.g. daemon start, profile applied, power state change) should still use `LOG.info()` or `LOG.warning()`.

---

## 🧹 Task Instructions

1. **Search for all debug print/log statements** in all Python source files.
2. **Wrap each with a check** to `config.is_debug_enabled()`.
3. **Ensure `config.py` is imported** in each file doing debug output.
4. **Avoid modifying config.yaml** or writing the debug flag to disk.
5. **DO NOT touch** journal-critical logs like startup banners or power change notices.

---

## 🗂️ Files to Review

The following files are known to contain debug statements:
- `dynamic_power.py` (daemon)
- `dynamic_power_user.py`
- `dynamic_power_command.py`
- `dynamic_power_session_helper.py`
- Any utility modules with verbose prints

---

## 📌 Design Philosophy

- Debug is **runtime only**.
- **No config changes** needed to enable/disable debug mode.
- Output to the terminal only if debugging is on.
- Journal logs remain clean unless it’s a critical event.

---

## 📎 Related Design Documents (May Need Updating)

The following documents may be impacted and should be regenerated after this task:
- `dynamic_power_architecture_overview.md` (reference to debug logging location)
- `ONBOARDING.md` (how to enable debug mode)
- `DEBUGGING.md` (will be created to summarize final implementation)
