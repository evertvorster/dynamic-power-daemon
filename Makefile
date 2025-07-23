# Dynamic‑Power‑Daemon – project‑root Makefile
#
# Installs all components into correct system locations.
# Works both for direct installs (sudo make install) and packaging builds
# (make DESTDIR="$pkgdir" PREFIX=/usr install).
#
PYTHON  ?= python
PREFIX  ?= /usr
DESTDIR ?=

SRC_DIR          := python
MODULE_DIR       := dynamic_power
RESOURCE_DIR     := $(SRC_DIR)/resources
TEMPLATE_DIR     := share/dynamic-power

# Determine interpreter site‑packages path
PURELIB := $(shell $(PYTHON) - <<'EOF'
import sysconfig, sys
print(sysconfig.get_paths()['purelib'])
EOF
)

# Standard dirs
BINDIR                 := $(PREFIX)/bin
SYSTEMD_SYSTEM_DIR     := $(PREFIX)/lib/systemd/system
SYSTEMD_USER_DIR       := $(PREFIX)/lib/systemd/user
SYSTEMD_USER_PRESET    := $(PREFIX)/lib/systemd/user-preset
DBUS_SYSTEM_POLICY_DIR := $(PREFIX)/etc/dbus-1/system.d
SHARE_DIR              := /usr/share/dynamic-power

.PHONY: all install uninstall

all:
	@echo "Nothing to build. Use 'make install'."

install:
	@echo "# --- Python package -------------------------------------------------"
	install -d "$(DESTDIR)$(PURELIB)"
	cp -r $(SRC_DIR)/$(MODULE_DIR) "$(DESTDIR)$(PURELIB)/"

	@echo "# --- Executables ----------------------------------------------------"
	install -Dm755 $(SRC_DIR)/$(MODULE_DIR)/cli.py                       "$(DESTDIR)$(BINDIR)/dynamic_power"
	install -Dm755 $(SRC_DIR)/$(MODULE_DIR)/dynamic_power_user.py        "$(DESTDIR)$(BINDIR)/dynamic_power_user"
	install -Dm755 $(SRC_DIR)/$(MODULE_DIR)/dynamic_power_command.py     "$(DESTDIR)$(BINDIR)/dynamic_power_command"
	install -Dm755 $(SRC_DIR)/$(MODULE_DIR)/dynamic_power_session_helper.py \
		"$(DESTDIR)$(BINDIR)/dynamic_power_session_helper"

	@echo "# --- System daemon unit -------------------------------------------"
	install -Dm644 dynamic-power.service                                 "$(DESTDIR)$(SYSTEMD_SYSTEM_DIR)/dynamic_power.service"

	@echo "# --- User session units & preset ----------------------------------"
	install -Dm644 $(RESOURCE_DIR)/systemd-user/dynamic_power_session.service  \
		"$(DESTDIR)$(SYSTEMD_USER_DIR)/dynamic_power_session.service"
	install -Dm644 $(RESOURCE_DIR)/systemd-user/dynamic_power_command.service \
		"$(DESTDIR)$(SYSTEMD_USER_DIR)/dynamic_power_command.service"
	install -Dm644 $(RESOURCE_DIR)/systemd-user/90-dynamic-power.preset       \
		"$(DESTDIR)$(SYSTEMD_USER_PRESET)/90-dynamic-power.preset"

	@echo "# --- DBus policy ---------------------------------------------------"
	install -Dm644 $(RESOURCE_DIR)/dbus/org.dynamic_power.Daemon.conf         \
		"$(DESTDIR)$(DBUS_SYSTEM_POLICY_DIR)/org.dynamic_power.Daemon.conf"

	@echo "# --- YAML templates ------------------------------------------------"
	install -Dm644 $(TEMPLATE_DIR)/dynamic-power.yaml      "$(DESTDIR)$(SHARE_DIR)/dynamic-power.yaml"
	install -Dm644 $(TEMPLATE_DIR)/dynamic-power-user.yaml "$(DESTDIR)$(SHARE_DIR)/dynamic-power-user.yaml"

	@echo "Install complete."

uninstall:
	@echo "Removing installation (PREFIX=$(PREFIX))"
	@rm -vf "$(DESTDIR)$(BINDIR)"/dynamic_power*
	@rm -vrf "$(DESTDIR)$(PURELIB)/$(MODULE_DIR)"
	@rm -vf "$(DESTDIR)$(SYSTEMD_SYSTEM_DIR)/dynamic_power.service"
	@rm -vf "$(DESTDIR)$(SYSTEMD_USER_DIR)"/dynamic_power_{session,command}.service
	@rm -vf "$(DESTDIR)$(SYSTEMD_USER_PRESET)/90-dynamic-power.preset"
	@rm -vf "$(DESTDIR)$(DBUS_SYSTEM_POLICY_DIR)/org.dynamic_power.Daemon.conf"
	@rm -vf "$(DESTDIR)$(SHARE_DIR)"/dynamic-power*.yaml
	@echo "Uninstall complete."
