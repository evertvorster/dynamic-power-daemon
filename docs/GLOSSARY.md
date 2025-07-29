# Glossary of Terms

| Term               | Definition                                                                 |
|--------------------|----------------------------------------------------------------------------|
| **DBus**           | Inter-process communication system used to send signals between processes. |
| **EPP**            | Energy Performance Preference — kernel-level power behavior tuning.        |
| **SetProfile**     | DBus method to instruct daemon to change the CPU power profile.            |
| **SetLoadThresholds** | DBus method to set low/high load thresholds for profile switching.     |
| **Dynamic Profile**| Power profile selected based on load/power source.                         |
| **Override**       | A user or process-specified instruction that bypasses automatic behavior.  |
| **Power Source**   | AC or Battery, used to influence power profile decisions.                  |
| **YAML**           | Human-readable config file format used for system/user settings.           |
| **inotify**        | Linux file monitoring API used to detect config changes.                   |
| **Daemon**         | A background process (runs as root in this project).                       |
