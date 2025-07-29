# CHANGELOG

## [Unreleased]
- Add README.md
- Move all runtime state from YAML to DBus
- Add DBus query for current profile to daemon
- Improve config.py enforcement across all modules

## v1.0.0 - Initial Python Rewrite
- Rewrote daemon from Bash to Python
- Introduced modular config loading
- Implemented DBus control interface
- Added GUI tray controller with live graphs
- Added session helper to manage startup processes
