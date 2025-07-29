# Developer Onboarding and Release Checklist

## 🛠️ Getting Started

1. Clone the repository:
```bash
git clone https://github.com/evertvorster/dynamic-power-daemon.git
cd dynamic-power-daemon
```

2. Install:
```bash
sudo make install
```

3. Enable debug mode (optional):
```bash
export DYNAMIC_POWER_DEBUG=1
```

This will enable debug-level print/logging output across all modules.

4. Run `dynamic_power_session_helper.py` manually, or add to autostart to launch the user + GUI agents.

---

## ✅ Release Checklist

- [ ] Update `PROJECT_HISTORY_LOG.md`
- [ ] Tag and push Git version
- [ ] Verify AUR build works
- [ ] Confirm DBus functionality
- [ ] Ensure debug flag does not persist in config.yaml
