# Dynamic Power Daemon – Project History


---
### Source: `PROJECT_HISTORY_LOG.md`

# 🧾 Project History Log — `dynamic_power`

## ## 📅 2025-07-21 — Direct User Overrides and Process Match Export

## 📅 2025-07-21 — Direct User Overrides and Process Match Export

- **Feature / Fix**:
  - Implemented DBus support for user-initiated power profile overrides ("Boss override").
  - Ensured dynamic_power daemon respects `powersave` limit while on battery, unless explicitly overridden by the user.
  - Fixed a bug where profile names from `dynamic_power_user` were passed with incorrect capitalization (e.g. "Powersave" instead of "power-saver").
  - Added export of matched processes to be visualized in `dynamic_power_command`.
  - Separated matched process data into a dedicated YAML file to avoid race conditions.
  - Fixed GUI bug where clicking "Apply" in the "Monitor New Process" dialog did not save changes.
  - Ensured process override buttons now show visual changes when activated.
  - Enhanced DBus communication to distinguish between process-initiated and user-initiated requests.
  
- **Context**:
  - Part of bringing feature parity to the new Python implementation compared to the original Bash script.
  - Expanding beyond original functionality by adding richer communication between components (daemon, user script, and controller GUI).
  - Needed robust distinction between automatic process matches and manual user overrides to avoid incorrect behavior on battery.

- **Challenges / Failures**:
  - Frequent issues with incorrect Python indentation during edits.
  - Race condition where `dynamic_power_user` would overwrite its own state outputs.
  - Initial attempts to handle new process matches in GUI failed due to missing signal/slot wiring and config reload logic.
  - Overwriting of state files without preserving all intended data led to mismatched runtime info.
  - Logic for override fallback was incorrectly resetting dynamic mode state.
  - Missed import (`normalize_profile`) caused errors at runtime.
  - Errors from unmatched parentheses and syntax errors in DBus handler files.

- **Modules / Files Touched**:
  - `dynamic_power_user.py`
  - `__init__.py` (main daemon)
  - `dbus_interface.py`
  - `dynamic_power_command.py`

- **Lessons Learned**:
  - Python's indentation is brittle — even one space can cause silent bugs or syntax errors. Validate after every block change.
  - Separating dynamic state output into multiple files helps avoid data overwrite conflicts.
  - Always test DBus communication end-to-end (daemon ↔ user ↔ GUI) to confirm override types are respected.
  - GUI elements on pop-up dialogs (e.g. buttons) need manual wiring to ensure event callbacks are triggered.
  - Adding debug print/log statements early and interactively is crucial to verifying which logic paths execute.
  - Having a persistent override mode file (`dynamic_power_control.yaml`) gives the user full control outside of process-based logic.

---

## ## 📅 2025-07-21 — GUI Integration, Overrides, and Threshold Syncing

## 📅 2025-07-21 — GUI Integration, Overrides, and Threshold Syncing

- **Feature / Fix**:
  - Added proper exit handling for `dynamic_power_command` to clean up the `dynamic_power_user` subprocess.
  - Implemented logic in `dynamic_power_command` to write a new override state file at `/run/user/{uid}/dynamic_power_control.yaml`.
  - Added drag handlers to the threshold lines that update the config file (`~/.config/dynamic_power/config.yaml`) when thresholds are adjusted.
  - Verified that these threshold changes are picked up by `dynamic_power_user`, which then pushes them to the root daemon over DBus.
  - Added debug output toggled by `--debug` to relevant parts of the GUI and `dynamic_power_user`.
  - Implemented `read_control_override()` in `dynamic_power_user.py` to detect the desired override mode from the state file.
  - Applied behavior for `Inhibit Powersave`, `Performance`, `Balanced`, and `Powersave` modes in the absence of process matches.
  - Cleaned up indentation issues and ensured debug print statements are conditional on debug mode.

- **Context**:
  - These changes unify the GUI, user-space script, and daemon into a coherent system.
  - The override logic allows the user to take control of the power mode directly from the GUI, independent of background processes.

- **Challenges / Failures**:
  - Threshold values were initially not saved properly due to mismatched keys and default values being saved into the wrong part of the config.
  - A double-write bug in the config caused two separate `thresholds` sections.
  - Initial override file writing logic missed UID substitution and failed silently.
  - Some indentation errors in drag handlers broke runtime execution.
  - In one revision, important lines of the main function were accidentally deleted and had to be restored manually.

- **Modules / Files Touched**:
  - `dynamic_power_command.py`
    - GUI launch and shutdown handling
    - Override state file writer
    - Threshold drag-to-update logic
    - Conditional debug logging
  - `dynamic_power_user.py`
    - `read_control_override()` function
    - Override handling logic injected into main loop
    - Debug feedback for override actions
  - Config file: `~/.config/dynamic_power/config.yaml`
  - State file: `/run/user/{uid}/dynamic_power_control.yaml`

- **Lessons Learned**:
  - Always verify UID-based paths dynamically — do not hardcode assumptions.
  - Write debug output only when explicitly requested with `--debug` to avoid polluting logs or terminal.
  - Visual debug tools in GUI development (like drag event logs) are essential when troubleshooting invisible actions.
  - Maintain strict separation of responsibility between GUI, user-level controller, and root-level daemon.
  - Designate a state file for communication between GUI and user-level logic, and another between user and daemon — this ensures clarity and modularity.

---

## ## 📅 2025-07-22 — Panel Overdrive Toggle via Session Helper

## 📅 2025-07-22 — Panel Overdrive Toggle via Session Helper

- **Feature / Fix**:  
  Implemented automatic toggling of ASUS panel overdrive (`asusctl armoury panel_overdrive`) when switching between AC and battery power in the session helper.

- **Context**:  
  Phase 3 of the dynamic_power roadmap involved adjusting panel overdrive based on power source. The logic was to be handled entirely in the session helper (`dynamic_power_session_helper.py`), triggered by DBus `PowerStateChanged`.

- **Challenges / Failures** (if any):  
  - Early versions of the helper either failed to run (`Exec format error`) due to leading blank lines or failed silently due to incorrect detection of power state.  
  - ROG laptops exposed `ADP0/online`, but this was not always reliable for detecting real-time AC disconnection.  
  - Some builds incorrectly prioritized the `online` bit, missing fast `Discharging` transitions from the battery.  
  - Confusion over where the tray UI got its power state led to importing suboptimal logic into the helper.

- **Modules / Files Touched**:  
  - `dynamic_power_session_helper.py`: multiple revised versions based on clean GitHub base with different power detection strategies.
  - `config.py`: retained existing `get_panel_overdrive_config()` accessor.
  - No changes made to `dynamic_power_command.py` (used only as a reference for power detection).

- **Lessons Learned**:  
  - The most robust way to detect power source is to **prioritize battery status** (`BAT0/status`), falling back to `ADP0/online` only if needed.
  - The tray’s logic is simple and display-focused; helper code needs to be more responsive and resilient.
  - A blank line before the `#!/usr/bin/env python3` shebang causes `Exec format error` in systemd units.
  - Verbose logging is crucial when diagnosing hardware interactions like panel overdrive toggling.
  - Installing helper with `make install` correctly sets permissions, but behavior must be tested manually.

---

## ## 📅 2025-07-23 — Packaging Cleanup and Service Auto-Start

## 📅 2025-07-23 — Packaging Cleanup and Service Auto-Start

- **Feature / Fix**:
  - Refactored `Makefile` and `PKGBUILD` for proper Arch packaging of Python-based daemon and user tools.
  - Enabled automatic start of `dynamic_power.service` during install.
  - Fixed graphical session crash for tray UI (`dynamic_power_command.service`).

- **Context**:
  - The project transitioned from Bash to Python and required proper Arch-compliant packaging.
  - Earlier attempts to install Python files and systemd units did not use site-packages, causing integration problems.
  - `dynamic_power_command` crashed on startup due to missing graphical environment.
  - `dynamic_power.service` was not being activated on install due to missing `enable`.

- **Challenges / Failures** (if any):
  - Broken Makefile syntax due to `shell` function misuse.
  - Incorrect install paths (e.g., copying to `/usr/lib` instead of site-packages).
  - Qt tray app (`dynamic_power_command`) failed to start on boot because graphical session was not ready.
  - `dynamic_power_user` silently failed to control DBus due to the root daemon not running.

- **Modules / Files Touched**:
  - `Makefile` (moved to root, now uses `DESTDIR`, Python site-packages, install path detection).
  - `PKGBUILD` (rewritten to defer to Makefile, sets `pkgver()` and `pkgrel`, includes `.install` script).
  - `.SRCINFO` (regenerated).
  - `dynamic-power-daemon.install` (now enables the root systemd service automatically).
  - `resources/systemd-user/dynamic_power_command.service` (now `After=graphical-session.target` to prevent premature launch).

- **Lessons Learned**:
  - Always start systemd GUI apps with `After=graphical-session.target` to avoid X11 failures.
  - Use `python3 -m site --user-site` or `sysconfig.get_path()` to determine correct install location for Python modules.
  - `systemctl preset` only works when a matching preset file exists; if not, services must be explicitly enabled in `.install`.
  - During packaging, verify service status and logs after reboot to catch silent failures.
  - Packaged systemd services should be validated both at user and system level.

---

## ## 📅 2025-07-25 — Config Profile Fixes and Debug Cleanup

## 📅 2025-07-25 — Config Profile Fixes and Debug Cleanup

- **Feature / Fix**:
  - Fixed support for setting `"dynamic"` and `"responsive"` profiles from the process override editor in `dynamic_power_command`.
  - Ensured profiles are written to the config file in all-lowercase format, consistent with daemon expectations.
  - Fixed a startup crash caused by an undefined `proc` reference during button initialization.
  - Removed all no-op `try/except` blocks and obsolete debug print statements to simplify maintenance and readability.

- **Context**:
  - The GUI allowed users to assign power profiles to matched processes, but `"Dynamic"` and `"Responsive"` were not written properly to the config due to leftover logic from an earlier format that didn’t treat them as real profiles.
  - Debug statements scattered throughout the file were causing more confusion than clarity.
  - The user wanted a clean slate before continuing development.

- **Challenges / Failures**:
  - Startup error caused by incorrect context usage of `proc` during early UI setup.
  - Maintaining correct indentation and preserving behavior while stripping out `try/except` blocks and debug code.

- **Modules / Files Touched**:
  - `dynamic_power_command.py`

- **Lessons Learned**:
  - Legacy assumptions in config structure (like omitting `"dynamic"` and `"responsive"`) can persist and subtly break new behavior if not fully purged.
  - Consistently using lowercase for internal config values avoids case mismatch bugs.
  - Debugging logic should be centralized (e.g., via the `logging` module) rather than scattered print statements, to avoid breaking indentation or logic flow.
  - Removing empty `try/except` blocks helps surface real issues during development and encourages better long-term error handling.

---

## ## 📅 2025-07-25 — Dynamic Graph Scaling, Power Mode Profile Fixes, and Debug Overhaul

## 📅 2025-07-25 — Dynamic Graph Scaling, Power Mode Profile Fixes, and Debug Overhaul
- **Feature / Fix**:
  - Implemented dynamic vertical scaling for the system load graph (minimum 7, max = current load + 1).
  - Added accurate power state display (AC + battery status) to the GUI.
  - Replaced all debug print statements with a unified `dprint()` visual macro system.
  - Fixed a bug where power mode button text (e.g., “Performance”) was being written back incorrectly to the user config.
  - Supported five canonical power profiles: `dynamic`, `responsive`, `performance`, `balanced`, `powersave`.
  - Ensured edits to processes using “Dynamic” or “Responsive” are saved correctly.
  - Cleaned up indentation and fixed multiple syntax errors that were breaking runtime.

- **Context**:
  - The system load often exceeded the fixed graph scale of 10, so dynamic scaling was introduced.
  - Previously, process overrides in the config used `prevent_powersave: true`, which became incompatible with the new 5-mode system.
  - The GUI's process editing menu was inconsistently mapping profile text to config values.
  - Early troubleshooting was slowed by insufficient debug visibility and incorrect file paths.

- **Challenges / Failures**:
  - Numerous indentation errors due to inconsistent debug statement formats.
  - Multiple hours lost due to a mistyped config file name that silently failed to load `ac_id` and `battery_id`.
  - Dynamic scaling initially showed flatlines due to a bug in `update_graph()`.
  - Difficulty tracking which debug statements were executing due to lack of contextual info.
  - “Responsive” and “Dynamic” profiles weren’t being saved on Apply, requiring logic updates.

- **Modules / Files Touched**:
  - `dynamic_power_command.py`
  - `dynamic_power_user.py`
  - `dynamic-power-user.yaml` (new canonical format)

- **Lessons Learned**:
  - Always insert contextual debug output around variable reads and file accesses during troubleshooting.
  - Visual macros like `dprint()` are preferable for debug output — easier to enable/disable and standardize.
  - Standardizing power mode terminology across GUI and config makes future maintenance much simpler.
  - Don't assume config files are valid — confirm their existence and structure early in the code.
  - Avoid speculative code edits — verify the file contents and line numbers against actual source before refactoring.

---

## ## 📅 2025-07-25 — Dynamic Power Python Rewrite Finalization

## 📅 2025-07-25 — Dynamic Power Python Rewrite Finalization

- **Feature / Fix**: Migrated all core functionality from the original Bash script to Python.
- **Context**: Final goal was full feature parity with the original dynamic_power Bash daemon.
- **Challenges / Failures**:
  - Several small logic bugs during override handling.
  - File ownership and paths needed care due to user/root separation.
- **Modules / Files Touched**:
  - `dynamic_power_user.py`
  - `__init__.py`
- **Lessons Learned**:
  - Clear separation of responsibilities between root daemon and user-space process is crucial.
  - State sharing via `/run` files is clean and effective for root/user coordination.

---

## 📅 2025-07-25 — Process Priority Handling

- **Feature / Fix**: Added priority-based selection when multiple monitored processes are running.
- **Context**: Needed to resolve conflicts when multiple process overrides are active.
- **Challenges / Failures**:
  - Previous implementation did not always unset overrides correctly.
- **Modules / Files Touched**:
  - `dynamic_power_user.py`
- **Lessons Learned**:
  - Simpler is better: tracking active process and comparing by priority was clean and effective.

---

## 📅 2025-07-25 — DBus Cleanup and Bug Fixes

- **Feature / Fix**: Fixed bug where performance override wasn’t being unset when process exited.
- **Context**: Performance overrides were "sticking" due to stale state.
- **Challenges / Failures**:
  - Needed to track state across loops and send resets correctly.
- **Modules / Files Touched**:
  - `dynamic_power_user.py`
- **Lessons Learned**:
  - Track the name of the currently applied override to know when to unset.

---

## 📅 2025-07-25 — Dynamic State Sharing

- **Feature / Fix**: Created a YAML state file for dynamic power status sharing.
- **Context**: Needed shared file for communication between `dynamic_power_user` and the upcoming GUI.
- **Challenges / Failures**:
  - Initial file path decisions (resolved by placing user and root state in separate files).
- **Modules / Files Touched**:
  - `dynamic_power_user.py`
  - `__init__.py` (daemon)
- **Lessons Learned**:
  - Using `/run` and `/run/user/$UID` simplifies IPC between user and root.

---

## 📅 2025-07-25 — File Monitoring with `inotify`

- **Feature / Fix**: Implemented config file reloading with `inotify`.
- **Context**: Needed efficient, low-latency updates in `dynamic_power_user`.
- **Challenges / Failures**:
  - None; integration with `inotify_simple` was smooth.
- **Modules / Files Touched**:
  - `dynamic_power_user.py`
- **Lessons Learned**:
  - inotify is perfect for low-cost monitoring of config files.

---

## 📅 2025-07-25 — GUI Utility (dynamic_power_command) Bootstrapped

- **Feature / Fix**: Created full Qt-based GUI tray utility.
- **Context**: Replace legacy Bash-based monitor with a modern Qt GUI.
- **Challenges / Failures**:
  - Debugging draggable threshold lines.
  - Managing indentation bugs in generated code.
- **Modules / Files Touched**:
  - `dynamic_power_command.py`
- **Lessons Learned**:
  - PyQt + pyqtgraph provides enough power to implement responsive, dynamic tools.

---

## 📅 2025-07-25 — GUI Threshold Visualization and Editing

- **Feature / Fix**: Added draggable threshold lines on the graph.
- **Context**: Allow live reconfiguration of thresholds from the GUI.
- **Challenges / Failures**:
  - Initial lines not appearing due to missing layout triggers.
- **Modules / Files Touched**:
  - `dynamic_power_command.py`
- **Lessons Learned**:
  - Threshold lines must be updated after graph layout is established.

---

## 📅 2025-07-25 — Runtime Power State Display in GUI

- **Feature / Fix**: GUI now shows live system power mode and threshold values from state file.
- **Context**: Required to match previous Bash UI and support new dynamic state sharing.
- **Challenges / Failures**:
  - Ensuring threshold lines are only updated from state file, not overwriting config values.
- **Modules / Files Touched**:
  - `dynamic_power_command.py`
- **Lessons Learned**:
  - GUI-state desync can be avoided by separating runtime updates from config edits.

---

## 📅 2025-07-25 — Packaging and Deployment

- **Feature / Fix**: Updated Makefile and PKGBUILD to handle both daemon and user utilities.
- **Context**: Prepare the project for AUR release.
- **Challenges / Failures**:
  - Handling DESTDIR and PKGDIR consistently across all components.
- **Modules / Files Touched**:
  - `Makefile`
  - `PKGBUILD`
- **Lessons Learned**:
  - Respecting Arch packaging conventions pays off for seamless installation.

---

## ## 📅 2025-07-25 — GUI Enhancements: Process Matching and Power Source Display

## 📅 2025-07-25 — GUI Enhancements: Process Matching and Power Source Display

- **Feature / Fix**:
  - Added dynamic color highlighting to process buttons in `dynamic_power_command`:
    - Deep yellow for actively matched processes.
    - Pastel yellow for inactive but matched processes.
    - Reset to default when unmatched.
  - Added live AC/Battery power source status display below the graph and above the profile mode button.
  - Center-aligned the power status text.
  - Integrated `/etc/dynamic_power.yaml` for resolving `ac_id` and `battery_id`.

- **Context**:
  - The user wanted the GUI to reflect real-time system and process state information:
    - Power source (AC or battery).
    - Charging status.
    - Matched processes based on dynamic_power_user monitoring.
  - This aligns with the goal of making `dynamic_power_command` a comprehensive user-friendly control panel.

- **Challenges / Failures** (if any):
  - Initial patches introduced a Python `IndentationError` due to malformed or incomplete `if` blocks.
  - Writing to `/etc` was attempted during config creation, which failed due to lack of root permissions.
  - One intermediate version tried to blend logic for reading and writing configs, causing confusion and failure at runtime.
  - Merged blocks from prior versions led to broken logic during load-time fallback handling.

- **Modules / Files Touched**:
  - `dynamic_power_command.py` (several patched versions)
    - Added `get_power_info()` to read `/sys/class/power_supply/...` files.
    - Introduced new `QLabel` for displaying power state.
    - Modified `update_state()` to update power status live.
    - Added timer to re-check process match state and update button styles.
    - Corrected `load_config()` logic to merge values from `/etc/dynamic_power.yaml` without trying to write to it.

- **Lessons Learned**:
  - Always test patched GUI logic for Python indentation and flow — even minor structural issues can cause runtime failures.
  - Never attempt writes to privileged locations like `/etc` from user-level scripts.
  - When merging config values from multiple sources (user + system), ensure read-only fallback logic is clear and isolated.
  - Adding small, real-time GUI indicators (like process match colors and power source status) can dramatically improve usability and system visibility.

---

## ## 📅 2025-07-25 — Match File Output and Cleanup in `dynamic_power_user.py`

## 📅 2025-07-25 — Match File Output and Cleanup in `dynamic_power_user.py`
- **Feature / Fix**:
  - Implemented YAML output to `/run/user/{uid}/dynamic_power_matches.yaml` containing matched processes, their priorities, and active status.
  - Ensured the match file is deleted when no matching processes are found, preventing stale data.
  
- **Context**:
  - The user utility `dynamic_power_user.py` was enhanced to communicate its internal match state to the rest of the system (e.g. GUI or monitoring tools).
  - Previously, match detection worked and profile overrides were applied via DBus, but no record was kept of which process triggered the override.
  
- **Challenges / Failures**:
  - Initial implementation placed match file deletion inside a conditional block that was never reached if override mode was active (e.g. `manual_override: Inhibit Powersave`).
  - Fix required understanding of how `check_processes()` short-circuits execution in the presence of override modes.
  
- **Modules / Files Touched**:
  - `dynamic_power_user.py`:
    - Added match file writing logic inside the `if matches:` block.
    - Moved file deletion logic just after the match list is constructed, before conditional branching begins.

- **Lessons Learned**:
  - Avoid placing file write/delete logic deep inside conditional branches if it's expected to run regardless of outcome.
  - The override logic in `check_processes()` can short-circuit execution — any side effects (like file cleanup) must be placed before early `return` points or outside conditional paths.
  - Logging and structured YAML output are effective for debugging and integration with GUI tools.

---

## ## 📅 2025-07-25 — User Utility Integration and Process-Based Power Control

## 📅 2025-07-25 — User Utility Integration and Process-Based Power Control

- **Feature / Fix**:
  - Developed `dynamic_power_user.py` to run as a background user-level script.
  - Sends load thresholds to the `dynamic_power` daemon over DBus.
  - Reads per-user config file (`~/.config/dynamic_power/config.yaml`) to define process-specific power behavior.
  - Implements `--debug` mode to enable verbose logging for troubleshooting.
  - Detects and logs running processes that match configured overrides.
  - Logs detection events only once per process in normal mode, with reset if process disappears.
  - Defaults to 5-second polling interval if none is specified in the config.
  - Added proper journald integration using `systemd.journal.send`.
  - Fixed control flow and loop stability to exit cleanly on Ctrl+C.

- **Context**:
  - Enables per-user customization of power management behavior based on running applications (e.g. `obs`, `steam`).
  - Offloads user-specific policy decisions from the root daemon, preserving separation of concerns.
  - Provides system-friendly journald logging and debug control for visibility.

- **Challenges / Failures** (if any):
  - Missing `time.sleep()` initially caused tight infinite loop and unresponsive termination.
  - Improper config file structure (list vs dict) led to runtime exceptions.
  - Incorrect shebang placement (after a blank line) caused shell misinterpretation.
  - Early versions lacked `yaml` and `dbus` imports, breaking execution.
  - Logging initially did not appear in journalctl due to missing `systemd.journal.send`.

- **Modules / Files Touched**:
  - `dynamic_power_user.py` (new script, developed and debugged iteratively)
  - `/usr/share/dynamic-power/dynamic-power-user.yaml` (template config)
  - `Makefile` (previously updated to install the script as `/usr/bin/dynamic_power_user`)

- **Lessons Learned**:
  - User-configurable polling intervals and structured overrides provide great flexibility with minimal complexity.
  - Logging to both stdout and journald with debug toggles improves developer UX.
  - Systemd-compatible scripts must begin with the shebang — no preceding lines.
  - YAML structure must be validated early to avoid subtle runtime bugs.
  - Detecting both process start and stop is key to consistent behavior tracking.

---

## 2025-07-19 — Major Refactor: Remove EPP, Journal Logging, User Config

## 📅 2025-07-19 — Major Refactor: Remove EPP, Journal Logging, User Config

- **Feature / Fix**:
  - Removed all EPP and manual CPU governor logic.
  - Switched to using `powerprofilesctl` exclusively for power profile management.
  - Integrated systemd journal logging using Python’s logging module.
  - Added support for per-user configuration overrides.
  - Added graceful shutdown handling for clean systemd termination.
  - Split user and system configuration responsibilities.
  - Verified that KDE applet and `powerprofilesctl` reflect only EPP changes, not governor ones.

- **Context**:
  - The EPP mode detection bug led to discovery that setting powerprofiles too early locks EPP options.
  - Replaced EPP and governor manipulation with powerprofilesctl for simplicity and better system integration.
  - Integrated logging to system journal instead of `print()` or separate logs.
  - Introduced clean shutdown handling and signal support in `__init__.py`.
  - Identified the need for separating user preferences from system config, and added merge logic to support user overrides.

- **Challenges / Failures**:
  - Circular import issue between `debug.py`, `utils.py`, and `power_profiles.py`.
  - EPP detection was limited to “performance” due to setting the profile too early.
  - `power_profiles.py` was not updated correctly; multiple versions provided were identical.
  - User config was not loaded due to incorrect file path or directory mismatch.
  - YAML escape characters caused a syntax error in config.py.
  - Logging showed "No user config found" despite the file being present — due to incorrect path logic.

- **Modules / Files Touched**:
  - `__init__.py`: Removed EPP logic, added journal logging, added signal handling.
  - `epp.py`: Deprecated.
  - `debug.py`: Rewritten to log to systemd journal using `logging`.
  - `config.py`: Updated to load user config from `~/.config/dynamic_power/config.yaml`, merge values, and log merged values.
  - `power_profiles.py`: Cleaned up logging, removed unused sysfs path reads, retained profile aliases.
  - `utils.py`: Added structured logs.
  - `Makefile`: Now installs both config templates to `/usr/share/dynamic-power/`.
  - YAML config files: system config stripped of process overrides, user config added with per-process power policy.

- **Lessons Learned**:
  - Avoid setting powerprofiles too early to prevent locking out EPP modes.
  - Prefer system tools (`powerprofilesctl`) for compatibility and UX integration (KDE).
  - Always verify generated patches against uploaded files to avoid stale code issues.
  - Structured logging with systemd makes debugging much easier than scattered prints.
  - User config paths must be exact and expanded (e.g., using `os.path.expanduser()`).
  - Clean separation between system and user configuration greatly improves flexibility.

---

## 2025-07-19-dynamic_power_summary

## 📅 2025-07-19 — Major Refactor: Remove EPP, Journal Logging, User Config

- **Feature / Fix**:
  - Removed all EPP and manual CPU governor logic.
  - Switched to using `powerprofilesctl` exclusively for power profile management.
  - Integrated systemd journal logging using Python’s logging module.
  - Added support for per-user configuration overrides.
  - Added graceful shutdown handling for clean systemd termination.
  - Split user and system configuration responsibilities.
  - Verified that KDE applet and `powerprofilesctl` reflect only EPP changes, not governor ones.

- **Context**:
  - The EPP mode detection bug led to discovery that setting powerprofiles too early locks EPP options.
  - Replaced EPP and governor manipulation with powerprofilesctl for simplicity and better system integration.
  - Integrated logging to system journal instead of `print()` or separate logs.
  - Introduced clean shutdown handling and signal support in `__init__.py`.
  - Identified the need for separating user preferences from system config, and added merge logic to support user overrides.

- **Challenges / Failures**:
  - Circular import issue between `debug.py`, `utils.py`, and `power_profiles.py`.
  - EPP detection was limited to “performance” due to setting the profile too early.
  - `power_profiles.py` was not updated correctly; multiple versions provided were identical.
  - User config was not loaded due to incorrect file path or directory mismatch.
  - YAML escape characters caused a syntax error in config.py.
  - Logging showed "No user config found" despite the file being present — due to incorrect path logic.

- **Modules / Files Touched**:
  - `__init__.py`: Removed EPP logic, added journal logging, added signal handling.
  - `epp.py`: Deprecated.
  - `debug.py`: Rewritten to log to systemd journal using `logging`.
  - `config.py`: Updated to load user config from `~/.config/dynamic_power/config.yaml`, merge values, and log merged values.
  - `power_profiles.py`: Cleaned up logging, removed unused sysfs path reads, retained profile aliases.
  - `utils.py`: Added structured logs.
  - `Makefile`: Now installs both config templates to `/usr/share/dynamic-power/`.
  - YAML config files: system config stripped of process overrides, user config added with per-process power policy.

- **Lessons Learned**:
  - Avoid setting powerprofiles too early to prevent locking out EPP modes.
  - Prefer system tools (`powerprofilesctl`) for compatibility and UX integration (KDE).
  - Always verify generated patches against uploaded files to avoid stale code issues.
  - Structured logging with systemd makes debugging much easier than scattered prints.
  - User config paths must be exact and expanded (e.g., using `os.path.expanduser()`).
  - Clean separation between system and user configuration greatly improves flexibility.

---

## 2025-07-22 — Session Helper + Panel Overdrive Support

## 📅 2025-07-22 — Session Helper + Panel Overdrive Support

- **Feature / Fix**:
  - Implemented `dynamic_power_session_helper` for managing user-space session tasks.
  - Introduced systemd user units for clean startup of all user-side components.
  - Added support for panel overdrive toggling based on AC/battery state in the daemon.
  - Set up infrastructure for future sensor/control integration using DBus between user apps.

- **Context**:
  - The existing system used autostart for `dynamic_power_command`; this was replaced by a systemd user unit.
  - A new session helper was created to act as a launcher and manager of the user-side processes (`dynamic_power_user`, future `sensors`).
  - Introduced `panel.overdrive.enable_on_ac` config setting (version bumped to v6).
  - Added logic to `dynamic_power` daemon to parse this new config and apply panel overdrive via `asusctl`.
  - Investigated display refresh rate and other ASUS-specific firmware toggles as future expansion paths.
  - Began abstracting communication between user utilities into a shared DBus service (`org.dynamic_power.UserBus`).

- **Challenges / Failures**:
  - Multiple broken download links and file not found errors due to transfer errors and stale references.
  - Python syntax errors and incorrect shebang lines caused repeated failures during service startup.
  - `dbus_next.sync` import failed on Arch Linux (v0.2.3) — workaround used fallback import with version check.
  - `dynamic_power_command` initially failed to show tray icon due to service dying silently on startup error.

- **Modules / Files Touched**:
  - `dynamic_power_command.py`
  - `dynamic_power_session_helper.py` (new)
  - `/usr/lib/systemd/user/dynamic_power_session.service` (new)
  - `/usr/lib/systemd/user/dynamic_power_command.service` (new)
  - `dynamic-power.yaml` (updated to version 6 with panel config)
  - `Makefile` (install/uninstall logic for new services and helper)

- **Lessons Learned**:
  - Always test download links before using valuable response tokens.
  - File comparison is critical — wrong files lead to wasted debugging effort.
  - `dbus_next.sync` is not always available — fallback to async or default import.
  - Systemd user services offer a clean, reliable alternative to desktop autostart.
  - Modular design via DBus interfaces helps reduce coupling and improves testability.
  - Minor features like panel overdrive are easiest to implement once system state and config propagation are well-structured.

---

## 2025-07-23

📅 2025-07-23 — Power Status Fix and Panel Overdrive Attempt
Feature / Fix:

Added fallback logic to delay startup of dynamic_power_command until the X11 display and DBus session are available.

Investigated why the GUI was not updating battery status upon power change.

Identified a race condition and restructured systemd units to improve service dependencies and startup timing.

Explored and tested triggering panel overdrive switching via rog-control-center.

Context:

dynamic_power_command was crashing on startup due to Qt's inability to connect to the X11 display. This was due to systemd launching the service before the session environment was fully ready.

At the same time, the user noticed that battery state detection was broken — likely related to the same initialization timing.

It was discovered that when rog-control-center (rogcc) is restarted after dynamic_power_command, the UI properly updates and begins controlling panel overdrive too — indicating the issue was order-of-startup related.

Challenges / Failures:

A circular dependency arose between dynamic_power_command and dynamic_power_session, when both were tied to graphical-session.target.

Setting After=dynamic_power_session.service caused systemd to delete jobs to break the cycle.

Disabling the services was complicated by the fact that systemd had enabled them in the global user scope, causing them to restart even after --user disable.

Modules / Files Touched:

dynamic_power_command.service (systemd unit)

dynamic_power_session.service (systemd unit)

dynamic_power_command.py (Qt application, updated with fallback wait logic)

Lessons Learned:

Qt-based apps relying on xcb and DBus must not start immediately at login — they require the full graphical session environment.

Avoid circular dependencies in systemd units, especially when using Requires= and After= together.

Use systemctl --global disable to truly prevent a user service from starting automatically.

Launch order of services like rog-control-center can impact shared system state (e.g., panel overdrive settings).

Introducing a built-in startup delay or a DBus/X11 wait loop is often more reliable than trying to force ordering via systemd.

---

## 2025-07-25 — Dynamic Power — Feature Integrations & UI Sync

📅 2025-07-25 — Dynamic Power — Feature Integrations & UI Sync
Feature / Fix: Finalized panel_overdrive feature integration and UI synchronization

Context: After resolving earlier bugs and confirming that panel overdrive switching works when launched via autostart, the focus shifted to clearly defining when a feature is considered "done." This was driven by the need to surface feature state in the UI and tie feature logic into DBus communication.

Challenges / Failures:

DBus communication initially failed silently under certain startup conditions, traced to environmental differences between autostart and terminal sessions.

File-not-found errors and session truncations interrupted documentation workflow and forced manual restarts.

ChatGPT session instability required re-uploads of markdown files and slowed progress.

Modules / Files Touched:

dynamic_power_command.py: Intended target for displaying UI state for panel overdrive.

DBus interface module (no filename change) — reference design reused from power state logic.

Markdown files:

screen_refresh_feature.md — completion criteria and implementation planning

FEATURE_TOGGLES.md — centralized description of when features are considered complete

Lessons Learned:

Each feature is now defined as complete when:

It has a toggle in the UI.

Its current state is reflected in the UI via DBus.

It can be turned on/off from the config file.

It reacts to power state changes if required.

DBus communication must be tested under autostart, not just command-line conditions.

Project markdown files should be saved locally after each major edit, to guard against session memory loss.

---

## dynamic_power_debug_session_summary

## 📅 2025-07-25 — Debug Mode & Stability Fixes

- **Feature / Fix**:
  - Introduced `--debug` command-line switch to enable detailed debug output.
  - Expanded debug logging across all major modules to trace input/output variables and internal logic.
  - Added runtime CPU frequency profile detection.
  - Fixed critical import errors and restored `normalize_profile` alias support.
  - Restored and clarified logic in `__init__.py` for initial profile setting and polling interval.

- **Context**:
  - These changes were introduced to improve observability of the daemon and help debug complex behaviors during feature development.
  - This marks the return to a fully functional baseline after earlier issues caused by inconsistent file versions and untracked edits.

- **Challenges / Failures (if any)**:
  - Initial implementations removed important logic unintentionally (e.g., polling interval, power profile init).
  - Confusion caused by filename mismatches and overwritten or renamed download files (e.g., `epp(1).py` instead of `epp.py`).
  - A major source of bugs was memory drift and editing from outdated assumptions.

- **Modules / Files Touched**:
  - `__init__.py`
  - `config.py`
  - `epp.py`
  - `power_profiles.py`
  - `debug.py` (new)
  - All files now honor the global `--debug` flag via `DEBUG_ENABLED`

- **Lessons Learned**:
  - Always derive new versions from explicitly uploaded files to avoid regression and confusion.
  - Provide full files for each code change, especially in critical infrastructure projects.
  - Keep debug toggles centralized and consistent, and structure imports to avoid circular dependencies.
  - Use Git branches and commits to isolate experimental changes and maintain stable mainlines.

---

## dynamic_power_history_2025-07

## 📅 2025-07-16 — Investigating CPU Frequency Caps
- **Feature / Fix**: Investigated and identified cause of capped CPU frequencies in balanced mode.
- **Context**: Observed CPU frequency cap at ~2 GHz despite balanced mode. Found it persisted due to `scaling_max_freq` not matching hardware maximum.
- **Challenges / Failures**: Initial suspicion that `powerprofilesctl` or governor logic might be limiting frequency; deeper inspection showed it was a sysfs cap.
- **Modules / Files Touched**: `dynamic_power.sh`
- **Lessons Learned**: Always verify `scaling_max_freq` against `cpuinfo_max_freq` at startup; not all power profile switches reset scaling limits.

## 📅 2025-07-16 — Added Startup Frequency Fix
- **Feature / Fix**: Added code to `dynamic_power.sh` to switch to performance mode at startup and restore `scaling_max_freq`.
- **Context**: Ensures system starts at full speed before entering main loop; especially useful during boot-time system services and tasks.
- **Challenges / Failures**: Needed short performance window (~10s) for system stabilization.
- **Modules / Files Touched**: `dynamic_power.sh` (`fix_scaling_max_freqs()` logic)
- **Lessons Learned**: A brief switch to performance mode on boot improves responsiveness without long-term power cost.

## 📅 2025-07-17 — Panel Overdrive Integration (Partial)
- **Feature / Fix**: Began integrating ASUS panel overdrive toggle based on AC/Battery state.
- **Context**: Overdrive can be controlled via sysfs (`panel_overdrive/current_value`). Not all ASUS systems support this.
- **Challenges / Failures**: Needed to preserve overdrive state on unplug, restore on replug; config had no section for ASUS features yet.
- **Modules / Files Touched**: `dynamic_power.sh`, `/etc/dynamic_power.conf`
- **Lessons Learned**: System-specific features like overdrive require conditional logic and persistence across state changes.

## 📅 2025-07-17 — Config Auto-Upgrade Implemented
- **Feature / Fix**: Implemented auto-upgrade for config files missing new variables.
- **Context**: Older configs lacked `PANEL_OVERDRIVE_*` variables; now automatically regenerated with comments while preserving current settings.
- **Challenges / Failures**: Quoting and escaping issues (e.g., backslashes added to process list). Needed to comment all config keys for clarity.
- **Modules / Files Touched**: `dynamic_power.sh` config handling block.
- **Lessons Learned**: Clear, documented configs reduce confusion and help track new features; regeneration must preserve existing values.

## 📅 2025-07-17 — Bug Fixes and Feature Restoration
- **Feature / Fix**: Fixed regression where performance mode startup logic was skipped after adding config regeneration. Restored `set_profile performance` and `sleep 10`.
- **Context**: Performance window was missing, slowing boot-time processes. Panel overdrive still did not persist properly across battery changes.
- **Challenges / Failures**: Race condition between reading and setting overdrive state; bugs in ASUS profile detection using `asusctl`.
- **Modules / Files Touched**: `dynamic_power.sh`, `set_profile()` logic.
- **Lessons Learned**: Each added feature must preserve existing behavior; test profile support early to prevent unexpected DBus or CLI errors.

## 📅 2025-07-17 — ASUSCTL Profile Detection Regression
- **Feature / Fix**: Diagnosed `asusctl` profile error — `LowPower` not supported despite being listed. Confirmed previous use of `powerprofilesctl` instead.
- **Context**: System reported `org.freedesktop.DBus.Error.NotSupported` when calling `asusctl profile quiet`.
- **Challenges / Failures**: Asusctl CLI interface changed slightly; error handling needed improvement; fallback to `powerprofilesctl` clarified.
- **Modules / Files Touched**: `set_profile()` function in `dynamic_power.sh`
- **Lessons Learned**: Device and distro updates can break assumptions; always test external tool behavior before relying on it in scripts.

## 📅 2025-07-17 — Planning Python Rewrite
- **Feature / Fix**: Decided to rewrite the daemon in Python to improve maintainability and modularity.
- **Context**: `dynamic_power.sh` had grown large and hard to maintain; adding features without regressions became difficult.
- **Challenges / Failures**: Bash not ideal for modular code; Arch packaging policies prohibit `pip install` dependencies.
- **Modules / Files Touched**: N/A (planning stage)
- **Lessons Learned**: Plan for scale — long-running utilities benefit from structured code and typed configuration (YAML chosen over JSON for readability).

---

---
### Source: `PROJECT_HISTORY_LOG.md`

# 🧾 Project History Log — `dynamic_power`

## ## 📅 2025-07-21 — Direct User Overrides and Process Match Export

## 📅 2025-07-21 — Direct User Overrides and Process Match Export

- **Feature / Fix**:
  - Implemented DBus support for user-initiated power profile overrides ("Boss override").
  - Ensured dynamic_power daemon respects `powersave` limit while on battery, unless explicitly overridden by the user.
  - Fixed a bug where profile names from `dynamic_power_user` were passed with incorrect capitalization (e.g. "Powersave" instead of "power-saver").
  - Added export of matched processes to be visualized in `dynamic_power_command`.
  - Separated matched process data into a dedicated YAML file to avoid race conditions.
  - Fixed GUI bug where clicking "Apply" in the "Monitor New Process" dialog did not save changes.
  - Ensured process override buttons now show visual changes when activated.
  - Enhanced DBus communication to distinguish between process-initiated and user-initiated requests.
  
- **Context**:
  - Part of bringing feature parity to the new Python implementation compared to the original Bash script.
  - Expanding beyond original functionality by adding richer communication between components (daemon, user script, and controller GUI).
  - Needed robust distinction between automatic process matches and manual user overrides to avoid incorrect behavior on battery.

- **Challenges / Failures**:
  - Frequent issues with incorrect Python indentation during edits.
  - Race condition where `dynamic_power_user` would overwrite its own state outputs.
  - Initial attempts to handle new process matches in GUI failed due to missing signal/slot wiring and config reload logic.
  - Overwriting of state files without preserving all intended data led to mismatched runtime info.
  - Logic for override fallback was incorrectly resetting dynamic mode state.
  - Missed import (`normalize_profile`) caused errors at runtime.
  - Errors from unmatched parentheses and syntax errors in DBus handler files.

- **Modules / Files Touched**:
  - `dynamic_power_user.py`
  - `__init__.py` (main daemon)
  - `dbus_interface.py`
  - `dynamic_power_command.py`

- **Lessons Learned**:
  - Python's indentation is brittle — even one space can cause silent bugs or syntax errors. Validate after every block change.
  - Separating dynamic state output into multiple files helps avoid data overwrite conflicts.
  - Always test DBus communication end-to-end (daemon ↔ user ↔ GUI) to confirm override types are respected.
  - GUI elements on pop-up dialogs (e.g. buttons) need manual wiring to ensure event callbacks are triggered.
  - Adding debug print/log statements early and interactively is crucial to verifying which logic paths execute.
  - Having a persistent override mode file (`dynamic_power_control.yaml`) gives the user full control outside of process-based logic.

---

## ## 📅 2025-07-21 — GUI Integration, Overrides, and Threshold Syncing

## 📅 2025-07-21 — GUI Integration, Overrides, and Threshold Syncing

- **Feature / Fix**:
  - Added proper exit handling for `dynamic_power_command` to clean up the `dynamic_power_user` subprocess.
  - Implemented logic in `dynamic_power_command` to write a new override state file at `/run/user/{uid}/dynamic_power_control.yaml`.
  - Added drag handlers to the threshold lines that update the config file (`~/.config/dynamic_power/config.yaml`) when thresholds are adjusted.
  - Verified that these threshold changes are picked up by `dynamic_power_user`, which then pushes them to the root daemon over DBus.
  - Added debug output toggled by `--debug` to relevant parts of the GUI and `dynamic_power_user`.
  - Implemented `read_control_override()` in `dynamic_power_user.py` to detect the desired override mode from the state file.
  - Applied behavior for `Inhibit Powersave`, `Performance`, `Balanced`, and `Powersave` modes in the absence of process matches.
  - Cleaned up indentation issues and ensured debug print statements are conditional on debug mode.

- **Context**:
  - These changes unify the GUI, user-space script, and daemon into a coherent system.
  - The override logic allows the user to take control of the power mode directly from the GUI, independent of background processes.

- **Challenges / Failures**:
  - Threshold values were initially not saved properly due to mismatched keys and default values being saved into the wrong part of the config.
  - A double-write bug in the config caused two separate `thresholds` sections.
  - Initial override file writing logic missed UID substitution and failed silently.
  - Some indentation errors in drag handlers broke runtime execution.
  - In one revision, important lines of the main function were accidentally deleted and had to be restored manually.

- **Modules / Files Touched**:
  - `dynamic_power_command.py`
    - GUI launch and shutdown handling
    - Override state file writer
    - Threshold drag-to-update logic
    - Conditional debug logging
  - `dynamic_power_user.py`
    - `read_control_override()` function
    - Override handling logic injected into main loop
    - Debug feedback for override actions
  - Config file: `~/.config/dynamic_power/config.yaml`
  - State file: `/run/user/{uid}/dynamic_power_control.yaml`

- **Lessons Learned**:
  - Always verify UID-based paths dynamically — do not hardcode assumptions.
  - Write debug output only when explicitly requested with `--debug` to avoid polluting logs or terminal.
  - Visual debug tools in GUI development (like drag event logs) are essential when troubleshooting invisible actions.
  - Maintain strict separation of responsibility between GUI, user-level controller, and root-level daemon.
  - Designate a state file for communication between GUI and user-level logic, and another between user and daemon — this ensures clarity and modularity.

---

## ## 📅 2025-07-22 — Panel Overdrive Toggle via Session Helper

## 📅 2025-07-22 — Panel Overdrive Toggle via Session Helper

- **Feature / Fix**:  
  Implemented automatic toggling of ASUS panel overdrive (`asusctl armoury panel_overdrive`) when switching between AC and battery power in the session helper.

- **Context**:  
  Phase 3 of the dynamic_power roadmap involved adjusting panel overdrive based on power source. The logic was to be handled entirely in the session helper (`dynamic_power_session_helper.py`), triggered by DBus `PowerStateChanged`.

- **Challenges / Failures** (if any):  
  - Early versions of the helper either failed to run (`Exec format error`) due to leading blank lines or failed silently due to incorrect detection of power state.  
  - ROG laptops exposed `ADP0/online`, but this was not always reliable for detecting real-time AC disconnection.  
  - Some builds incorrectly prioritized the `online` bit, missing fast `Discharging` transitions from the battery.  
  - Confusion over where the tray UI got its power state led to importing suboptimal logic into the helper.

- **Modules / Files Touched**:  
  - `dynamic_power_session_helper.py`: multiple revised versions based on clean GitHub base with different power detection strategies.
  - `config.py`: retained existing `get_panel_overdrive_config()` accessor.
  - No changes made to `dynamic_power_command.py` (used only as a reference for power detection).

- **Lessons Learned**:  
  - The most robust way to detect power source is to **prioritize battery status** (`BAT0/status`), falling back to `ADP0/online` only if needed.
  - The tray’s logic is simple and display-focused; helper code needs to be more responsive and resilient.
  - A blank line before the `#!/usr/bin/env python3` shebang causes `Exec format error` in systemd units.
  - Verbose logging is crucial when diagnosing hardware interactions like panel overdrive toggling.
  - Installing helper with `make install` correctly sets permissions, but behavior must be tested manually.

---

## ## 📅 2025-07-23 — Packaging Cleanup and Service Auto-Start

## 📅 2025-07-23 — Packaging Cleanup and Service Auto-Start

- **Feature / Fix**:
  - Refactored `Makefile` and `PKGBUILD` for proper Arch packaging of Python-based daemon and user tools.
  - Enabled automatic start of `dynamic_power.service` during install.
  - Fixed graphical session crash for tray UI (`dynamic_power_command.service`).

- **Context**:
  - The project transitioned from Bash to Python and required proper Arch-compliant packaging.
  - Earlier attempts to install Python files and systemd units did not use site-packages, causing integration problems.
  - `dynamic_power_command` crashed on startup due to missing graphical environment.
  - `dynamic_power.service` was not being activated on install due to missing `enable`.

- **Challenges / Failures** (if any):
  - Broken Makefile syntax due to `shell` function misuse.
  - Incorrect install paths (e.g., copying to `/usr/lib` instead of site-packages).
  - Qt tray app (`dynamic_power_command`) failed to start on boot because graphical session was not ready.
  - `dynamic_power_user` silently failed to control DBus due to the root daemon not running.

- **Modules / Files Touched**:
  - `Makefile` (moved to root, now uses `DESTDIR`, Python site-packages, install path detection).
  - `PKGBUILD` (rewritten to defer to Makefile, sets `pkgver()` and `pkgrel`, includes `.install` script).
  - `.SRCINFO` (regenerated).
  - `dynamic-power-daemon.install` (now enables the root systemd service automatically).
  - `resources/systemd-user/dynamic_power_command.service` (now `After=graphical-session.target` to prevent premature launch).

- **Lessons Learned**:
  - Always start systemd GUI apps with `After=graphical-session.target` to avoid X11 failures.
  - Use `python3 -m site --user-site` or `sysconfig.get_path()` to determine correct install location for Python modules.
  - `systemctl preset` only works when a matching preset file exists; if not, services must be explicitly enabled in `.install`.
  - During packaging, verify service status and logs after reboot to catch silent failures.
  - Packaged systemd services should be validated both at user and system level.

---

## ## 📅 2025-07-25 — Config Profile Fixes and Debug Cleanup

## 📅 2025-07-25 — Config Profile Fixes and Debug Cleanup

- **Feature / Fix**:
  - Fixed support for setting `"dynamic"` and `"responsive"` profiles from the process override editor in `dynamic_power_command`.
  - Ensured profiles are written to the config file in all-lowercase format, consistent with daemon expectations.
  - Fixed a startup crash caused by an undefined `proc` reference during button initialization.
  - Removed all no-op `try/except` blocks and obsolete debug print statements to simplify maintenance and readability.

- **Context**:
  - The GUI allowed users to assign power profiles to matched processes, but `"Dynamic"` and `"Responsive"` were not written properly to the config due to leftover logic from an earlier format that didn’t treat them as real profiles.
  - Debug statements scattered throughout the file were causing more confusion than clarity.
  - The user wanted a clean slate before continuing development.

- **Challenges / Failures**:
  - Startup error caused by incorrect context usage of `proc` during early UI setup.
  - Maintaining correct indentation and preserving behavior while stripping out `try/except` blocks and debug code.

- **Modules / Files Touched**:
  - `dynamic_power_command.py`

- **Lessons Learned**:
  - Legacy assumptions in config structure (like omitting `"dynamic"` and `"responsive"`) can persist and subtly break new behavior if not fully purged.
  - Consistently using lowercase for internal config values avoids case mismatch bugs.
  - Debugging logic should be centralized (e.g., via the `logging` module) rather than scattered print statements, to avoid breaking indentation or logic flow.
  - Removing empty `try/except` blocks helps surface real issues during development and encourages better long-term error handling.

---

## ## 📅 2025-07-25 — Dynamic Graph Scaling, Power Mode Profile Fixes, and Debug Overhaul

## 📅 2025-07-25 — Dynamic Graph Scaling, Power Mode Profile Fixes, and Debug Overhaul
- **Feature / Fix**:
  - Implemented dynamic vertical scaling for the system load graph (minimum 7, max = current load + 1).
  - Added accurate power state display (AC + battery status) to the GUI.
  - Replaced all debug print statements with a unified `dprint()` visual macro system.
  - Fixed a bug where power mode button text (e.g., “Performance”) was being written back incorrectly to the user config.
  - Supported five canonical power profiles: `dynamic`, `responsive`, `performance`, `balanced`, `powersave`.
  - Ensured edits to processes using “Dynamic” or “Responsive” are saved correctly.
  - Cleaned up indentation and fixed multiple syntax errors that were breaking runtime.

- **Context**:
  - The system load often exceeded the fixed graph scale of 10, so dynamic scaling was introduced.
  - Previously, process overrides in the config used `prevent_powersave: true`, which became incompatible with the new 5-mode system.
  - The GUI's process editing menu was inconsistently mapping profile text to config values.
  - Early troubleshooting was slowed by insufficient debug visibility and incorrect file paths.

- **Challenges / Failures**:
  - Numerous indentation errors due to inconsistent debug statement formats.
  - Multiple hours lost due to a mistyped config file name that silently failed to load `ac_id` and `battery_id`.
  - Dynamic scaling initially showed flatlines due to a bug in `update_graph()`.
  - Difficulty tracking which debug statements were executing due to lack of contextual info.
  - “Responsive” and “Dynamic” profiles weren’t being saved on Apply, requiring logic updates.

- **Modules / Files Touched**:
  - `dynamic_power_command.py`
  - `dynamic_power_user.py`
  - `dynamic-power-user.yaml` (new canonical format)

- **Lessons Learned**:
  - Always insert contextual debug output around variable reads and file accesses during troubleshooting.
  - Visual macros like `dprint()` are preferable for debug output — easier to enable/disable and standardize.
  - Standardizing power mode terminology across GUI and config makes future maintenance much simpler.
  - Don't assume config files are valid — confirm their existence and structure early in the code.
  - Avoid speculative code edits — verify the file contents and line numbers against actual source before refactoring.

---

## ## 📅 2025-07-25 — Dynamic Power Python Rewrite Finalization

## 📅 2025-07-25 — Dynamic Power Python Rewrite Finalization

- **Feature / Fix**: Migrated all core functionality from the original Bash script to Python.
- **Context**: Final goal was full feature parity with the original dynamic_power Bash daemon.
- **Challenges / Failures**:
  - Several small logic bugs during override handling.
  - File ownership and paths needed care due to user/root separation.
- **Modules / Files Touched**:
  - `dynamic_power_user.py`
  - `__init__.py`
- **Lessons Learned**:
  - Clear separation of responsibilities between root daemon and user-space process is crucial.
  - State sharing via `/run` files is clean and effective for root/user coordination.

---

## 📅 2025-07-25 — Process Priority Handling

- **Feature / Fix**: Added priority-based selection when multiple monitored processes are running.
- **Context**: Needed to resolve conflicts when multiple process overrides are active.
- **Challenges / Failures**:
  - Previous implementation did not always unset overrides correctly.
- **Modules / Files Touched**:
  - `dynamic_power_user.py`
- **Lessons Learned**:
  - Simpler is better: tracking active process and comparing by priority was clean and effective.

---

## 📅 2025-07-25 — DBus Cleanup and Bug Fixes

- **Feature / Fix**: Fixed bug where performance override wasn’t being unset when process exited.
- **Context**: Performance overrides were "sticking" due to stale state.
- **Challenges / Failures**:
  - Needed to track state across loops and send resets correctly.
- **Modules / Files Touched**:
  - `dynamic_power_user.py`
- **Lessons Learned**:
  - Track the name of the currently applied override to know when to unset.

---

## 📅 2025-07-25 — Dynamic State Sharing

- **Feature / Fix**: Created a YAML state file for dynamic power status sharing.
- **Context**: Needed shared file for communication between `dynamic_power_user` and the upcoming GUI.
- **Challenges / Failures**:
  - Initial file path decisions (resolved by placing user and root state in separate files).
- **Modules / Files Touched**:
  - `dynamic_power_user.py`
  - `__init__.py` (daemon)
- **Lessons Learned**:
  - Using `/run` and `/run/user/$UID` simplifies IPC between user and root.

---

## 📅 2025-07-25 — File Monitoring with `inotify`

- **Feature / Fix**: Implemented config file reloading with `inotify`.
- **Context**: Needed efficient, low-latency updates in `dynamic_power_user`.
- **Challenges / Failures**:
  - None; integration with `inotify_simple` was smooth.
- **Modules / Files Touched**:
  - `dynamic_power_user.py`
- **Lessons Learned**:
  - inotify is perfect for low-cost monitoring of config files.

---

## 📅 2025-07-25 — GUI Utility (dynamic_power_command) Bootstrapped

- **Feature / Fix**: Created full Qt-based GUI tray utility.
- **Context**: Replace legacy Bash-based monitor with a modern Qt GUI.
- **Challenges / Failures**:
  - Debugging draggable threshold lines.
  - Managing indentation bugs in generated code.
- **Modules / Files Touched**:
  - `dynamic_power_command.py`
- **Lessons Learned**:
  - PyQt + pyqtgraph provides enough power to implement responsive, dynamic tools.

---

## 📅 2025-07-25 — GUI Threshold Visualization and Editing

- **Feature / Fix**: Added draggable threshold lines on the graph.
- **Context**: Allow live reconfiguration of thresholds from the GUI.
- **Challenges / Failures**:
  - Initial lines not appearing due to missing layout triggers.
- **Modules / Files Touched**:
  - `dynamic_power_command.py`
- **Lessons Learned**:
  - Threshold lines must be updated after graph layout is established.

---

## 📅 2025-07-25 — Runtime Power State Display in GUI

- **Feature / Fix**: GUI now shows live system power mode and threshold values from state file.
- **Context**: Required to match previous Bash UI and support new dynamic state sharing.
- **Challenges / Failures**:
  - Ensuring threshold lines are only updated from state file, not overwriting config values.
- **Modules / Files Touched**:
  - `dynamic_power_command.py`
- **Lessons Learned**:
  - GUI-state desync can be avoided by separating runtime updates from config edits.

---

## 📅 2025-07-25 — Packaging and Deployment

- **Feature / Fix**: Updated Makefile and PKGBUILD to handle both daemon and user utilities.
- **Context**: Prepare the project for AUR release.
- **Challenges / Failures**:
  - Handling DESTDIR and PKGDIR consistently across all components.
- **Modules / Files Touched**:
  - `Makefile`
  - `PKGBUILD`
- **Lessons Learned**:
  - Respecting Arch packaging conventions pays off for seamless installation.

---

## ## 📅 2025-07-25 — GUI Enhancements: Process Matching and Power Source Display

## 📅 2025-07-25 — GUI Enhancements: Process Matching and Power Source Display

- **Feature / Fix**:
  - Added dynamic color highlighting to process buttons in `dynamic_power_command`:
    - Deep yellow for actively matched processes.
    - Pastel yellow for inactive but matched processes.
    - Reset to default when unmatched.
  - Added live AC/Battery power source status display below the graph and above the profile mode button.
  - Center-aligned the power status text.
  - Integrated `/etc/dynamic_power.yaml` for resolving `ac_id` and `battery_id`.

- **Context**:
  - The user wanted the GUI to reflect real-time system and process state information:
    - Power source (AC or battery).
    - Charging status.
    - Matched processes based on dynamic_power_user monitoring.
  - This aligns with the goal of making `dynamic_power_command` a comprehensive user-friendly control panel.

- **Challenges / Failures** (if any):
  - Initial patches introduced a Python `IndentationError` due to malformed or incomplete `if` blocks.
  - Writing to `/etc` was attempted during config creation, which failed due to lack of root permissions.
  - One intermediate version tried to blend logic for reading and writing configs, causing confusion and failure at runtime.
  - Merged blocks from prior versions led to broken logic during load-time fallback handling.

- **Modules / Files Touched**:
  - `dynamic_power_command.py` (several patched versions)
    - Added `get_power_info()` to read `/sys/class/power_supply/...` files.
    - Introduced new `QLabel` for displaying power state.
    - Modified `update_state()` to update power status live.
    - Added timer to re-check process match state and update button styles.
    - Corrected `load_config()` logic to merge values from `/etc/dynamic_power.yaml` without trying to write to it.

- **Lessons Learned**:
  - Always test patched GUI logic for Python indentation and flow — even minor structural issues can cause runtime failures.
  - Never attempt writes to privileged locations like `/etc` from user-level scripts.
  - When merging config values from multiple sources (user + system), ensure read-only fallback logic is clear and isolated.
  - Adding small, real-time GUI indicators (like process match colors and power source status) can dramatically improve usability and system visibility.

---

## ## 📅 2025-07-25 — Match File Output and Cleanup in `dynamic_power_user.py`

## 📅 2025-07-25 — Match File Output and Cleanup in `dynamic_power_user.py`
- **Feature / Fix**:
  - Implemented YAML output to `/run/user/{uid}/dynamic_power_matches.yaml` containing matched processes, their priorities, and active status.
  - Ensured the match file is deleted when no matching processes are found, preventing stale data.
  
- **Context**:
  - The user utility `dynamic_power_user.py` was enhanced to communicate its internal match state to the rest of the system (e.g. GUI or monitoring tools).
  - Previously, match detection worked and profile overrides were applied via DBus, but no record was kept of which process triggered the override.
  
- **Challenges / Failures**:
  - Initial implementation placed match file deletion inside a conditional block that was never reached if override mode was active (e.g. `manual_override: Inhibit Powersave`).
  - Fix required understanding of how `check_processes()` short-circuits execution in the presence of override modes.
  
- **Modules / Files Touched**:
  - `dynamic_power_user.py`:
    - Added match file writing logic inside the `if matches:` block.
    - Moved file deletion logic just after the match list is constructed, before conditional branching begins.

- **Lessons Learned**:
  - Avoid placing file write/delete logic deep inside conditional branches if it's expected to run regardless of outcome.
  - The override logic in `check_processes()` can short-circuit execution — any side effects (like file cleanup) must be placed before early `return` points or outside conditional paths.
  - Logging and structured YAML output are effective for debugging and integration with GUI tools.

---

## ## 📅 2025-07-25 — User Utility Integration and Process-Based Power Control

## 📅 2025-07-25 — User Utility Integration and Process-Based Power Control

- **Feature / Fix**:
  - Developed `dynamic_power_user.py` to run as a background user-level script.
  - Sends load thresholds to the `dynamic_power` daemon over DBus.
  - Reads per-user config file (`~/.config/dynamic_power/config.yaml`) to define process-specific power behavior.
  - Implements `--debug` mode to enable verbose logging for troubleshooting.
  - Detects and logs running processes that match configured overrides.
  - Logs detection events only once per process in normal mode, with reset if process disappears.
  - Defaults to 5-second polling interval if none is specified in the config.
  - Added proper journald integration using `systemd.journal.send`.
  - Fixed control flow and loop stability to exit cleanly on Ctrl+C.

- **Context**:
  - Enables per-user customization of power management behavior based on running applications (e.g. `obs`, `steam`).
  - Offloads user-specific policy decisions from the root daemon, preserving separation of concerns.
  - Provides system-friendly journald logging and debug control for visibility.

- **Challenges / Failures** (if any):
  - Missing `time.sleep()` initially caused tight infinite loop and unresponsive termination.
  - Improper config file structure (list vs dict) led to runtime exceptions.
  - Incorrect shebang placement (after a blank line) caused shell misinterpretation.
  - Early versions lacked `yaml` and `dbus` imports, breaking execution.
  - Logging initially did not appear in journalctl due to missing `systemd.journal.send`.

- **Modules / Files Touched**:
  - `dynamic_power_user.py` (new script, developed and debugged iteratively)
  - `/usr/share/dynamic-power/dynamic-power-user.yaml` (template config)
  - `Makefile` (previously updated to install the script as `/usr/bin/dynamic_power_user`)

- **Lessons Learned**:
  - User-configurable polling intervals and structured overrides provide great flexibility with minimal complexity.
  - Logging to both stdout and journald with debug toggles improves developer UX.
  - Systemd-compatible scripts must begin with the shebang — no preceding lines.
  - YAML structure must be validated early to avoid subtle runtime bugs.
  - Detecting both process start and stop is key to consistent behavior tracking.

---

## 2025-07-19 — Major Refactor: Remove EPP, Journal Logging, User Config

## 📅 2025-07-19 — Major Refactor: Remove EPP, Journal Logging, User Config

- **Feature / Fix**:
  - Removed all EPP and manual CPU governor logic.
  - Switched to using `powerprofilesctl` exclusively for power profile management.
  - Integrated systemd journal logging using Python’s logging module.
  - Added support for per-user configuration overrides.
  - Added graceful shutdown handling for clean systemd termination.
  - Split user and system configuration responsibilities.
  - Verified that KDE applet and `powerprofilesctl` reflect only EPP changes, not governor ones.

- **Context**:
  - The EPP mode detection bug led to discovery that setting powerprofiles too early locks EPP options.
  - Replaced EPP and governor manipulation with powerprofilesctl for simplicity and better system integration.
  - Integrated logging to system journal instead of `print()` or separate logs.
  - Introduced clean shutdown handling and signal support in `__init__.py`.
  - Identified the need for separating user preferences from system config, and added merge logic to support user overrides.

- **Challenges / Failures**:
  - Circular import issue between `debug.py`, `utils.py`, and `power_profiles.py`.
  - EPP detection was limited to “performance” due to setting the profile too early.
  - `power_profiles.py` was not updated correctly; multiple versions provided were identical.
  - User config was not loaded due to incorrect file path or directory mismatch.
  - YAML escape characters caused a syntax error in config.py.
  - Logging showed "No user config found" despite the file being present — due to incorrect path logic.

- **Modules / Files Touched**:
  - `__init__.py`: Removed EPP logic, added journal logging, added signal handling.
  - `epp.py`: Deprecated.
  - `debug.py`: Rewritten to log to systemd journal using `logging`.
  - `config.py`: Updated to load user config from `~/.config/dynamic_power/config.yaml`, merge values, and log merged values.
  - `power_profiles.py`: Cleaned up logging, removed unused sysfs path reads, retained profile aliases.
  - `utils.py`: Added structured logs.
  - `Makefile`: Now installs both config templates to `/usr/share/dynamic-power/`.
  - YAML config files: system config stripped of process overrides, user config added with per-process power policy.

- **Lessons Learned**:
  - Avoid setting powerprofiles too early to prevent locking out EPP modes.
  - Prefer system tools (`powerprofilesctl`) for compatibility and UX integration (KDE).
  - Always verify generated patches against uploaded files to avoid stale code issues.
  - Structured logging with systemd makes debugging much easier than scattered prints.
  - User config paths must be exact and expanded (e.g., using `os.path.expanduser()`).
  - Clean separation between system and user configuration greatly improves flexibility.

---

## 2025-07-19-dynamic_power_summary

## 📅 2025-07-19 — Major Refactor: Remove EPP, Journal Logging, User Config

- **Feature / Fix**:
  - Removed all EPP and manual CPU governor logic.
  - Switched to using `powerprofilesctl` exclusively for power profile management.
  - Integrated systemd journal logging using Python’s logging module.
  - Added support for per-user configuration overrides.
  - Added graceful shutdown handling for clean systemd termination.
  - Split user and system configuration responsibilities.
  - Verified that KDE applet and `powerprofilesctl` reflect only EPP changes, not governor ones.

- **Context**:
  - The EPP mode detection bug led to discovery that setting powerprofiles too early locks EPP options.
  - Replaced EPP and governor manipulation with powerprofilesctl for simplicity and better system integration.
  - Integrated logging to system journal instead of `print()` or separate logs.
  - Introduced clean shutdown handling and signal support in `__init__.py`.
  - Identified the need for separating user preferences from system config, and added merge logic to support user overrides.

- **Challenges / Failures**:
  - Circular import issue between `debug.py`, `utils.py`, and `power_profiles.py`.
  - EPP detection was limited to “performance” due to setting the profile too early.
  - `power_profiles.py` was not updated correctly; multiple versions provided were identical.
  - User config was not loaded due to incorrect file path or directory mismatch.
  - YAML escape characters caused a syntax error in config.py.
  - Logging showed "No user config found" despite the file being present — due to incorrect path logic.

- **Modules / Files Touched**:
  - `__init__.py`: Removed EPP logic, added journal logging, added signal handling.
  - `epp.py`: Deprecated.
  - `debug.py`: Rewritten to log to systemd journal using `logging`.
  - `config.py`: Updated to load user config from `~/.config/dynamic_power/config.yaml`, merge values, and log merged values.
  - `power_profiles.py`: Cleaned up logging, removed unused sysfs path reads, retained profile aliases.
  - `utils.py`: Added structured logs.
  - `Makefile`: Now installs both config templates to `/usr/share/dynamic-power/`.
  - YAML config files: system config stripped of process overrides, user config added with per-process power policy.

- **Lessons Learned**:
  - Avoid setting powerprofiles too early to prevent locking out EPP modes.
  - Prefer system tools (`powerprofilesctl`) for compatibility and UX integration (KDE).
  - Always verify generated patches against uploaded files to avoid stale code issues.
  - Structured logging with systemd makes debugging much easier than scattered prints.
  - User config paths must be exact and expanded (e.g., using `os.path.expanduser()`).
  - Clean separation between system and user configuration greatly improves flexibility.

---

## 2025-07-22 — Session Helper + Panel Overdrive Support

## 📅 2025-07-22 — Session Helper + Panel Overdrive Support

- **Feature / Fix**:
  - Implemented `dynamic_power_session_helper` for managing user-space session tasks.
  - Introduced systemd user units for clean startup of all user-side components.
  - Added support for panel overdrive toggling based on AC/battery state in the daemon.
  - Set up infrastructure for future sensor/control integration using DBus between user apps.

- **Context**:
  - The existing system used autostart for `dynamic_power_command`; this was replaced by a systemd user unit.
  - A new session helper was created to act as a launcher and manager of the user-side processes (`dynamic_power_user`, future `sensors`).
  - Introduced `panel.overdrive.enable_on_ac` config setting (version bumped to v6).
  - Added logic to `dynamic_power` daemon to parse this new config and apply panel overdrive via `asusctl`.
  - Investigated display refresh rate and other ASUS-specific firmware toggles as future expansion paths.
  - Began abstracting communication between user utilities into a shared DBus service (`org.dynamic_power.UserBus`).

- **Challenges / Failures**:
  - Multiple broken download links and file not found errors due to transfer errors and stale references.
  - Python syntax errors and incorrect shebang lines caused repeated failures during service startup.
  - `dbus_next.sync` import failed on Arch Linux (v0.2.3) — workaround used fallback import with version check.
  - `dynamic_power_command` initially failed to show tray icon due to service dying silently on startup error.

- **Modules / Files Touched**:
  - `dynamic_power_command.py`
  - `dynamic_power_session_helper.py` (new)
  - `/usr/lib/systemd/user/dynamic_power_session.service` (new)
  - `/usr/lib/systemd/user/dynamic_power_command.service` (new)
  - `dynamic-power.yaml` (updated to version 6 with panel config)
  - `Makefile` (install/uninstall logic for new services and helper)

- **Lessons Learned**:
  - Always test download links before using valuable response tokens.
  - File comparison is critical — wrong files lead to wasted debugging effort.
  - `dbus_next.sync` is not always available — fallback to async or default import.
  - Systemd user services offer a clean, reliable alternative to desktop autostart.
  - Modular design via DBus interfaces helps reduce coupling and improves testability.
  - Minor features like panel overdrive are easiest to implement once system state and config propagation are well-structured.

---

## 2025-07-23

📅 2025-07-23 — Power Status Fix and Panel Overdrive Attempt
Feature / Fix:

Added fallback logic to delay startup of dynamic_power_command until the X11 display and DBus session are available.

Investigated why the GUI was not updating battery status upon power change.

Identified a race condition and restructured systemd units to improve service dependencies and startup timing.

Explored and tested triggering panel overdrive switching via rog-control-center.

Context:

dynamic_power_command was crashing on startup due to Qt's inability to connect to the X11 display. This was due to systemd launching the service before the session environment was fully ready.

At the same time, the user noticed that battery state detection was broken — likely related to the same initialization timing.

It was discovered that when rog-control-center (rogcc) is restarted after dynamic_power_command, the UI properly updates and begins controlling panel overdrive too — indicating the issue was order-of-startup related.

Challenges / Failures:

A circular dependency arose between dynamic_power_command and dynamic_power_session, when both were tied to graphical-session.target.

Setting After=dynamic_power_session.service caused systemd to delete jobs to break the cycle.

Disabling the services was complicated by the fact that systemd had enabled them in the global user scope, causing them to restart even after --user disable.

Modules / Files Touched:

dynamic_power_command.service (systemd unit)

dynamic_power_session.service (systemd unit)

dynamic_power_command.py (Qt application, updated with fallback wait logic)

Lessons Learned:

Qt-based apps relying on xcb and DBus must not start immediately at login — they require the full graphical session environment.

Avoid circular dependencies in systemd units, especially when using Requires= and After= together.

Use systemctl --global disable to truly prevent a user service from starting automatically.

Launch order of services like rog-control-center can impact shared system state (e.g., panel overdrive settings).

Introducing a built-in startup delay or a DBus/X11 wait loop is often more reliable than trying to force ordering via systemd.

---

## 2025-07-25 — Dynamic Power — Feature Integrations & UI Sync

📅 2025-07-25 — Dynamic Power — Feature Integrations & UI Sync
Feature / Fix: Finalized panel_overdrive feature integration and UI synchronization

Context: After resolving earlier bugs and confirming that panel overdrive switching works when launched via autostart, the focus shifted to clearly defining when a feature is considered "done." This was driven by the need to surface feature state in the UI and tie feature logic into DBus communication.

Challenges / Failures:

DBus communication initially failed silently under certain startup conditions, traced to environmental differences between autostart and terminal sessions.

File-not-found errors and session truncations interrupted documentation workflow and forced manual restarts.

ChatGPT session instability required re-uploads of markdown files and slowed progress.

Modules / Files Touched:

dynamic_power_command.py: Intended target for displaying UI state for panel overdrive.

DBus interface module (no filename change) — reference design reused from power state logic.

Markdown files:

screen_refresh_feature.md — completion criteria and implementation planning

FEATURE_TOGGLES.md — centralized description of when features are considered complete

Lessons Learned:

Each feature is now defined as complete when:

It has a toggle in the UI.

Its current state is reflected in the UI via DBus.

It can be turned on/off from the config file.

It reacts to power state changes if required.

DBus communication must be tested under autostart, not just command-line conditions.

Project markdown files should be saved locally after each major edit, to guard against session memory loss.

---

## dynamic_power_debug_session_summary

## 📅 2025-07-25 — Debug Mode & Stability Fixes

- **Feature / Fix**:
  - Introduced `--debug` command-line switch to enable detailed debug output.
  - Expanded debug logging across all major modules to trace input/output variables and internal logic.
  - Added runtime CPU frequency profile detection.
  - Fixed critical import errors and restored `normalize_profile` alias support.
  - Restored and clarified logic in `__init__.py` for initial profile setting and polling interval.

- **Context**:
  - These changes were introduced to improve observability of the daemon and help debug complex behaviors during feature development.
  - This marks the return to a fully functional baseline after earlier issues caused by inconsistent file versions and untracked edits.

- **Challenges / Failures (if any)**:
  - Initial implementations removed important logic unintentionally (e.g., polling interval, power profile init).
  - Confusion caused by filename mismatches and overwritten or renamed download files (e.g., `epp(1).py` instead of `epp.py`).
  - A major source of bugs was memory drift and editing from outdated assumptions.

- **Modules / Files Touched**:
  - `__init__.py`
  - `config.py`
  - `epp.py`
  - `power_profiles.py`
  - `debug.py` (new)
  - All files now honor the global `--debug` flag via `DEBUG_ENABLED`

- **Lessons Learned**:
  - Always derive new versions from explicitly uploaded files to avoid regression and confusion.
  - Provide full files for each code change, especially in critical infrastructure projects.
  - Keep debug toggles centralized and consistent, and structure imports to avoid circular dependencies.
  - Use Git branches and commits to isolate experimental changes and maintain stable mainlines.

---

## dynamic_power_history_2025-07

## 📅 2025-07-16 — Investigating CPU Frequency Caps
- **Feature / Fix**: Investigated and identified cause of capped CPU frequencies in balanced mode.
- **Context**: Observed CPU frequency cap at ~2 GHz despite balanced mode. Found it persisted due to `scaling_max_freq` not matching hardware maximum.
- **Challenges / Failures**: Initial suspicion that `powerprofilesctl` or governor logic might be limiting frequency; deeper inspection showed it was a sysfs cap.
- **Modules / Files Touched**: `dynamic_power.sh`
- **Lessons Learned**: Always verify `scaling_max_freq` against `cpuinfo_max_freq` at startup; not all power profile switches reset scaling limits.

## 📅 2025-07-16 — Added Startup Frequency Fix
- **Feature / Fix**: Added code to `dynamic_power.sh` to switch to performance mode at startup and restore `scaling_max_freq`.
- **Context**: Ensures system starts at full speed before entering main loop; especially useful during boot-time system services and tasks.
- **Challenges / Failures**: Needed short performance window (~10s) for system stabilization.
- **Modules / Files Touched**: `dynamic_power.sh` (`fix_scaling_max_freqs()` logic)
- **Lessons Learned**: A brief switch to performance mode on boot improves responsiveness without long-term power cost.

## 📅 2025-07-17 — Panel Overdrive Integration (Partial)
- **Feature / Fix**: Began integrating ASUS panel overdrive toggle based on AC/Battery state.
- **Context**: Overdrive can be controlled via sysfs (`panel_overdrive/current_value`). Not all ASUS systems support this.
- **Challenges / Failures**: Needed to preserve overdrive state on unplug, restore on replug; config had no section for ASUS features yet.
- **Modules / Files Touched**: `dynamic_power.sh`, `/etc/dynamic_power.conf`
- **Lessons Learned**: System-specific features like overdrive require conditional logic and persistence across state changes.

## 📅 2025-07-17 — Config Auto-Upgrade Implemented
- **Feature / Fix**: Implemented auto-upgrade for config files missing new variables.
- **Context**: Older configs lacked `PANEL_OVERDRIVE_*` variables; now automatically regenerated with comments while preserving current settings.
- **Challenges / Failures**: Quoting and escaping issues (e.g., backslashes added to process list). Needed to comment all config keys for clarity.
- **Modules / Files Touched**: `dynamic_power.sh` config handling block.
- **Lessons Learned**: Clear, documented configs reduce confusion and help track new features; regeneration must preserve existing values.

## 📅 2025-07-17 — Bug Fixes and Feature Restoration
- **Feature / Fix**: Fixed regression where performance mode startup logic was skipped after adding config regeneration. Restored `set_profile performance` and `sleep 10`.
- **Context**: Performance window was missing, slowing boot-time processes. Panel overdrive still did not persist properly across battery changes.
- **Challenges / Failures**: Race condition between reading and setting overdrive state; bugs in ASUS profile detection using `asusctl`.
- **Modules / Files Touched**: `dynamic_power.sh`, `set_profile()` logic.
- **Lessons Learned**: Each added feature must preserve existing behavior; test profile support early to prevent unexpected DBus or CLI errors.

## 📅 2025-07-17 — ASUSCTL Profile Detection Regression
- **Feature / Fix**: Diagnosed `asusctl` profile error — `LowPower` not supported despite being listed. Confirmed previous use of `powerprofilesctl` instead.
- **Context**: System reported `org.freedesktop.DBus.Error.NotSupported` when calling `asusctl profile quiet`.
- **Challenges / Failures**: Asusctl CLI interface changed slightly; error handling needed improvement; fallback to `powerprofilesctl` clarified.
- **Modules / Files Touched**: `set_profile()` function in `dynamic_power.sh`
- **Lessons Learned**: Device and distro updates can break assumptions; always test external tool behavior before relying on it in scripts.

## 📅 2025-07-17 — Planning Python Rewrite
- **Feature / Fix**: Decided to rewrite the daemon in Python to improve maintainability and modularity.
- **Context**: `dynamic_power.sh` had grown large and hard to maintain; adding features without regressions became difficult.
- **Challenges / Failures**: Bash not ideal for modular code; Arch packaging policies prohibit `pip install` dependencies.
- **Modules / Files Touched**: N/A (planning stage)
- **Lessons Learned**: Plan for scale — long-running utilities benefit from structured code and typed configuration (YAML chosen over JSON for readability).

---

---
### Source: `Unified_Session_Knowledge_2025-07-27T20-05_verbose.md`

# ✅ Unified Session Knowledge — 2025-07-27T20:05+02:00

This document consolidates and preserves session history related to the successful implementation of the **Panel Overdrive Switching** feature in the `dynamic_power` project. It includes all lessons, architecture decisions, timeline, and relevant components modified.

---

## 📆 Timeline of Key Events

### 🗓 2025-07-25
- ✅ **Initial Goal Set**: Implement automatic ASUS panel overdrive switching.
- 🧪 First implementation of GUI checkbox and detection logic.
- 🔧 Added `get_panel_overdrive_status()` to `sensors.py`, parsing `asusctl armoury --help` output.
- ⚠️ Encountered issues with `update_metrics()` being missing in helper module.
- ✅ Fixed broken imports, removed duplicates, and confirmed debug output.
- ✅ Feature tested successfully: confirmed switching based on AC/BAT state.
- ✅ Declared feature **fully working**.

### 🗓 2025-07-27T10:44+01:00
- 🧠 **Naming Refactor**:
  - `panel_overdrive` (feature toggle) renamed to `auto_panel_overdrive` in config/UI.
  - `panel_overdrive` (hardware status) kept as runtime status.
- 🛠 Updated `MainWindow._on_auto_panel_overdrive_toggled()` to correctly reflect state.
- 🐛 Fixed prior bug where method body was one line, causing `SyntaxError`.

### 🗓 2025-07-27T12:51+01:00
- 🐞 Type mismatch bug discovered: GUI checkbox always saved `False`.
- ✅ Fixed by explicitly casting `Qt.Checked` to `int(state) == 2`.
- 🧠 Identified UI mismatch between config and hardware status after toggling.
- 💬 GUI label improved to clarify config vs status.
- 🛑 Session was terminated due to fabricated code from an unloaded zip — violating user’s cardinal rules.
- ⚠️ Emphasis added: **Truth over continuity**, no faking data/code.

---

## 🔧 Component Breakdown

### 🖥 GUI: `dynamic_power_command.py`
- Added checkbox labeled: `Auto Panel Overdrive : On/Off`.
- Reads and writes `features.auto_panel_overdrive` from `~/.config/dynamic_power/config.yaml`.
- Displays live status of panel overdrive in GUI (via DBus).
- Button reflects `asusctl` status at startup and after power events.
- Updated event handler ensures runtime and UI state match config file.

### 🧭 Session Helper: `dynamic_power_session_helper.py`
- Added logic to parse output and extract `panel_overdrive` state.
- Sends value over DBus alongside other metrics.
- Debug print added temporarily to validate values (clearly marked for removal).
- Import cleanup and logic guards added to ensure file compiles and runs.

### 🧪 Sensors: `sensors.py`
- Implements `get_panel_overdrive_status()`:
  - Parses `asusctl armoury --help` output.
  - Reads true panel overdrive state via:
  - Pattern `[0,(1)]` interpreted as True, `[(0),1]` as False.
- Can be called from helper and GUI.
- Returns live state, which is sent to GUI for status display.

---

## 🧠 Architecture Clarifications

| Concept                 | Meaning                                                                 |
|------------------------|-------------------------------------------------------------------------|
| `auto_panel_overdrive` | Boolean in config — enables/disables **automatic** toggling feature     |
| `panel_overdrive`      | Live **hardware** status — shown to user for reference only             |

---

## 🔍 Final Test Output (Previously Confirmed)

```
[DEBUG] Panel overdrive status detected: True
Power source changed AC → BAT
Running asusctl armoury panel_overdrive 0
panel_overdrive set to 0
[DEBUG] Panel overdrive status detected: False
```

---

## 🧠 Lessons Learned

1. ✅ Always verify that the function you're patching actually exists.
2. 🔁 Avoid inserting duplicate imports; keep the code clean.
3. 🔍 Distinguish clearly between *config state* and *runtime status*.
4. 🛑 Never proceed with assumptions — report missing/invalid data before acting.
5. 🧪 Always recompile after patching logic into helper modules.
6. 📏 Apply **cardinal rules** strictly: no broken patches, no fabricated content.
7. 🧹 Split complex debugging into sequential steps — no simultaneous patching.

---

## ✅ Final Outcome

The **Panel Overdrive Switching** feature is now:
- Complete
- Integrated
- Tested
- Actively updating system state and UI

```
Feature Status: ✅ COMPLETE
```

This milestone reflects adherence to architecture, GUI integration, system coordination, and compliance with the project's cardinal rules.

---
### Source: `session_knowledge_2025-07-27.md`

✅ Session Knowledge Update — 2025-07-27T23:48+02:00

🧠 Current Focus
- Debugging launch behavior of `dynamic_power_command.py`, especially:
  - Ensuring `dynamic_power_user` is spawned *once and in the correct location*.
  - Ensuring it terminates properly on app exit (`Ctrl+C` or quit).
  - Removing legacy or misplaced spawn attempts to avoid duplication.

🧪 Observations
- **Command-line launch** now spawns `dynamic_power_user`, but the process is **unmanaged** (not tied to app lifecycle).
- **The DBus connection error persists** on launch:
  ```
  Failed to connect to org.dynamic_power.UserBus: org.freedesktop.DBus.Error.ServiceUnknown: The name is not activatable
  ```
  This error is **expected** if `dynamic_power_user` is not already running or has not registered its DBus service.
- An earlier attempt **duplicated** the `subprocess.Popen(...)` call instead of **moving** it to the correct place (where it can be tracked and terminated).

🛠 Fixes Discussed
- `dynamic_power_command.py` should:
  - Spawn `dynamic_power_user` **only once**, in the `MainWindow` constructor (after UI elements and debug setup).
  - Store its `Popen` handle in `self.user_proc` for cleanup in the `app.aboutToQuit.connect(...)` handler in `main()`.
  - Not attempt to spawn it earlier or elsewhere.
- File must **retain original structure**, with only **minimal edits** as per cardinal rules.

🔍 Outstanding Task
- Carefully relocate the `dynamic_power_user` spawn logic to just after the debug/setup logic in `MainWindow.__init__`.
- Ensure it is **removed from the old location** to prevent duplication.
- Confirm behavior by:
  - Launching from command line with `--debug`
  - Ensuring `dynamic_power_user` appears in `ps aux`
  - Confirming it exits when `command` does.

Let me know when you're ready in the next session — we’ll pick up precisely from here.

---
### Source: `session_knowledge_2025-07-28.md`

# Session Knowledge – 2025-07-28T09:49+0000

## Objective
Fix the issue of an unintended user-owned `dynamic_power` daemon being launched alongside the intended root-owned systemd service version.

## Problem Summary
- A second instance of `dynamic_power` was showing up under the user's UID.
- This duplicate instance was being launched inadvertently by the `dynamic_power_session_helper.py` script.
- DBus introspection confirmed that the system-owned daemon was running correctly.
- However, despite this, the helper still proceeded to launch its own `dynamic_power` process.

## Diagnostic Actions Taken
1. Verified the system daemon was available on the system DBus via:
   ```bash
   busctl --system | grep dynamic_power
   ```
2. Confirmed that `dynamic_power_command.py` and `dynamic_power_user.py` do **not** spawn or `exec` `dynamic_power` themselves.
3. Confirmed `spawn_user_helper()` and `spawn_ui()` in `dynamic_power_session_helper.py` only launch the intended GUI and user helper.
4. Traced the issue back to the absence of a gatekeeping check for existing user-owned `dynamic_power` processes.

## Fix Applied
- A patch was inserted into the `main()` function of `dynamic_power_session_helper.py` to detect and avoid launching GUI or user helper if a user-owned `dynamic_power` process is already running.
- `psutil` was used to iterate over processes and check for `name == "dynamic_power"` and matching username.

## Patch Details

### Imports Added
```python
import psutil
import getpass
```

### Patch Inserted at Start of `main()`
```python
username = getpass.getuser()
for proc in psutil.process_iter(['pid', 'name', 'username']):
    if proc.info['name'] == 'dynamic_power' and proc.info['username'] == username:
        LOG.warning("Detected unexpected user-owned dynamic_power process. Skipping launch.")
        return
```

## Supporting Fixes
- Confirmed that the custom `name_has_owner()` logic using `MessageBus.call()` works correctly to detect system daemon on DBus.
- Fixed a quoting bug in the Makefile `uninstall` target.

## Validation Steps
- Verified logs via:
  ```bash
  journalctl -b | grep dynamic_power
  ```
- Verified that only a **single root-owned** instance of `dynamic_power` is now running after reboot and relaunch.
- Verified that the session helper now respects the running system daemon and does not spawn a duplicate.

---
### Source: `dynamic_power_session_2025-07-28.md`

# 📝 Dynamic Power Daemon – Session Knowledge  
**📅 Date:** 2025-07-28  
**🕰 ISO Timestamp:** `2025-07-28T13:20+02:00`  
**🎯 Purpose:** Root-cause and resolve the mysterious user-owned `dynamic_power` process, confirm session launch behavior, and solidify launcher behavior across roles.  

---

## ✅ Primary Bug Resolved  
### **Symptom:**  
- When `dynamic_power_session_helper` was launched, it reported:
  ```
  Detected unexpected user-owned dynamic_power process. Skipping launch.
  ```
  ...even though no such process existed before launch.

### **Root Cause:**  
- The helper **spawned the GUI and user script**, and also appeared to spawn an **extra user-owned `dynamic_power` process**, seemingly violating the rule that `dynamic_power` must only be run by root.
- However, **none of the code explicitly launches** another `dynamic_power` instance.

### **Finding:**  
- The problem was due to **missing or incorrect `setproctitle` usage** in `dynamic_power_session_helper.py`.
- As a result, the helper itself **did not set its own process name**, and its presence was **misdetected as a user-owned `dynamic_power`** process by its own check:
  ```python
  if proc.info['name'] == 'dynamic_power' and proc.info['username'] == username:
  ```

### ✅ **Fix Implemented:**
```python
try:
    import setproctitle
    setproctitle.setproctitle('dynamic_power_session_helper')
except ImportError:
    try:
        import ctypes
        libc = ctypes.CDLL(None)
        libc.prctl(15, b'dynamic_power_session_helper', 0, 0, 0)
    except Exception:
        pass
```

---

## 🛠️ Files Involved & Key Changes

### 🔧 `/usr/bin/dynamic_power`
- No longer points to a `cli.py` launcher.
- Instead, `__init__.py` is now the daemon entry point.
- Includes this shebang:
  ```python
  #!/usr/bin/env python3
  ```
- Imports updated to use **relative imports** (e.g., `from . import config`) to allow module integrity.

### 📂 `Makefile`
- Updated to install `__init__.py` as `/usr/bin/dynamic_power`.
- `cli.py` is permanently removed and not referenced.
- Verified install/uninstall works after transition.

---

## 🧠 Behavioral Confirmations

### 👥 Correct Process Tree (Final State)
```
root       9549   1     0   ?    00:00:01 dynamic_power
evert     13212   5307  7   pts/2 00:00:00 dynamic_power_session_helper
evert     13214   13212 4   pts/2 00:00:00 dynamic_power_user
evert     13215   13212 15  pts/2 00:00:00 dynamic_power_command
```
- ✅ Root daemon launched by systemd.
- ✅ Session helper launched manually (or by desktop entry).
- ✅ Correctly spawns and tracks user+GUI helpers.
- ✅ No spurious user-owned `dynamic_power` processes.

### 🔍 Audit + Investigation
- Audit watch via `auditctl` confirmed no unexpected executions.
- `DBus` service discovery showed no `.service` auto-launchers involved.
- System journal confirmed that `dynamic_power` is only launched by systemd.

---

## 🪛 Next Session Objective

We were previously working on:
- **Panel Overdrive** feature
  - Ensuring the helper *respects* the toggle to enable/disable the feature.
  - Behavior should be centralized in `sensors.py` via `get_panel_overdrive_status(cfg=None)` as previously discussed.

---

## 🧩 Helpful Greps from Last Session

### Grep for `/usr/bin/dynamic_power`:
- No unexpected hardcoded invocations remain.
- All uses in `dynamic_power_session_helper.py` and `.desktop`/`.service` files are correct.

---

## ✅ Final Verification Logs

### `stat` checks:
```bash
$ stat -c "%y %n" /usr/bin/dynamic_power
2025-07-28 12:46:57.365283436 +0200 /usr/bin/dynamic_power

$ stat -c "%y %n" /usr/bin/dynamic_power_command
2025-07-28 12:46:57.372303114 +0200 /usr/bin/dynamic_power_command
```

### `file` output:
```bash
/usr/bin/dynamic_power:         Python script, ASCII text executable
/usr/bin/dynamic_power_command: Python script, Unicode text, UTF-8 text executable
```

### `readlink` & `head`:
```bash
$ readlink -f /usr/bin/dynamic_power_command
/usr/bin/dynamic_power_command

$ head -n 1 /usr/bin/dynamic_power_command
#!/usr/bin/env python3
```

---

## 📘 Notes

- We now use `setproctitle` consistently across all user-space tools.
- Launcher processes are correctly named, making `ps` output and debugging more reliable.
- This cleaned up the entire power session launch system — a major milestone!

---

🛌 **You're good to go for the next bug hunt. Sleep well, foundation’s solid.**

---
### Source: `Session_Knowledge_2025-07-27T21-40.md`

# Session Knowledge Update – 2025-07-27T23:05+0200

## Features Investigated

### Panel Overdrive Handling
- Confirmed that panel overdrive status is retrieved via `get_panel_overdrive_status()` in `sensors.py`
- Setting panel overdrive is done using `asusctl profile -p 1` or `0`.
- The session helper was modified to avoid calling `get_panel_overdrive_status()` or toggling the hardware if the feature `auto_panel_overdrive` is disabled.
- When the feature is off, it should report `"Disabled"` in the UI instead of `"On"` or `"Off"`.

## Implementation Architecture Review

### Script Launching Behavior
- Confirmed current architecture is:
  - `dynamic_power_command` launches both:
    - `dynamic_power_user`
    - `dynamic_power_session_helper`
- There was an implementation bug: `cmd` was not defined when launching `dynamic_power_user`, causing a runtime `NameError`.
- Fixed by adding:
  ```python
  cmd = ["/usr/bin/dynamic_power_user"]
  if self.debug_mode:
      cmd.append("--debug")
  ```

### Logging and Debug
- Current print/debug behavior is inconsistent across scripts.
- Agreed that future improvement is needed to unify debug handling using a log level or config-based verbosity toggle.
- Temporary solution: direct all debug output to terminal when `--debug` is passed.

## Troubleshooting Notes
- Feature toggle for `auto_panel_overdrive` correctly updates config file.
- However, session helper’s `update_metrics()` wasn't triggering as expected.
- Eventually determined the root cause was that the session helper wasn’t being launched due to the undefined `cmd` bug in `dynamic_power_command`.
- Once fixed, correct startup behavior should resume.

---

## 🧭 Future Plans

- Improve handling of **Panel Overdrive** and **KDE Panel Auto-hide**.
- Implement **fan curve control via asusctl** (optional future feature).
- Expand **GUI control** via `dynamic_power_command`, including visual indicators and override controls.
- Introduce **state persistence and user overrides** via `/run/user/{uid}/dynamic_power_control.yaml`.
- Support **clean session management** through `dynamic_power_session_helper`.
- Continue applying the **Cardinal Development Rules** for clean design and separation of logic.
