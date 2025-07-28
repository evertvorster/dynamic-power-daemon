# Dynamic‑Power‑Daemon – project‑root Makefile
#
PYTHON  ?= python
PREFIX  ?= /usr
DESTDIR ?=

SRC_DIR          := python
MODULE_DIR       := dynamic_power
RESOURCE_DIR     := $(SRC_DIR)/resources
TEMPLATE_DIR     := $(SRC_DIR)/share/dynamic-power

PURELIB := $(shell $(PYTHON) -c "import sysconfig, sys; print(sysconfig.get_paths()['purelib'])")

BINDIR                 := $(PREFIX)/bin
SYSTEMD_SYSTEM_DIR     := $(PREFIX)/lib/systemd/system
SYSTEMD_USER_DIR       := $(PREFIX)/lib/systemd/user
SYSTEMD_USER_PRESET    := $(PREFIX)/lib/systemd/user-preset
DBUS_SYSTEM_POLICY_DIR := /etc/dbus-1/system.d
SHARE_DIR              := /usr/share/dynamic-power
DESKTOP_DIR            := $(PREFIX)/share/applications
PIXMAPS_DIR 	       := $(PREFIX)/share/pixmaps

.PHONY: all install uninstall

all:
	@echo "Nothing to build. Use 'make install'."

install:
	@echo "# --- Python package -------------------------------------------------"
	install -d "$(DESTDIR)$(PURELIB)"
	cp -r $(SRC_DIR)/$(MODULE_DIR) "$(DESTDIR)$(PURELIB)/"

	@echo "# --- Executables ----------------------------------------------------"
	install -Dm755 $(SRC_DIR)/$(MODULE_DIR)/dynamic_power_launcher.py    "$(DESTDIR)$(BINDIR)/dynamic_power"
	install -Dm755 $(SRC_DIR)/$(MODULE_DIR)/dynamic_power_user.py        "$(DESTDIR)$(BINDIR)/dynamic_power_user"
	install -Dm755 $(SRC_DIR)/$(MODULE_DIR)/dynamic_power_command.py     "$(DESTDIR)$(BINDIR)/dynamic_power_command"
	install -Dm755 $(SRC_DIR)/$(MODULE_DIR)/dynamic_power_session_helper.py \
		"$(DESTDIR)$(BINDIR)/dynamic_power_session_helper"

	@echo "# --- System daemon unit -------------------------------------------"
	install -Dm644 dynamic-power.service                                 "$(DESTDIR)$(SYSTEMD_SYSTEM_DIR)/dynamic_power.service"

	@echo "# --- DBus policy ---------------------------------------------------"
	install -Dm644 $(RESOURCE_DIR)/dbus/org.dynamic_power.Daemon.conf \
		"$(DESTDIR)$(DBUS_SYSTEM_POLICY_DIR)/org.dynamic_power.Daemon.conf"

	@echo "# --- Desktop entry --------------------------------------------------"
	install -Dm644 $(RESOURCE_DIR)/dynamic-power.desktop \
		"$(DESTDIR)$(DESKTOP_DIR)/dynamic-power.desktop"

	@echo "# --- Icon -----------------------------------------------------------"
	install -Dm644 $(RESOURCE_DIR)/dynamic-power.svg "$(DESTDIR)$(PIXMAPS_DIR)/dynamic-power.svg"

	@echo "# --- YAML templates ------------------------------------------------"
	install -Dm644 $(TEMPLATE_DIR)/dynamic-power.yaml      "$(DESTDIR)$(SHARE_DIR)/dynamic-power.yaml"
	install -Dm644 $(TEMPLATE_DIR)/dynamic-power-user.yaml "$(DESTDIR)$(SHARE_DIR)/dynamic-power-user.yaml"

	@echo "Install complete."

uninstall:
	@echo "Removing installation (PREFIX=$(PREFIX))"
	@rm -vf "$(DESTDIR)$(BINDIR)"/dynamic_power*
	@rm -vrf "$(DESTDIR)$(PURELIB)/$(MODULE_DIR)"
	@rm -vf "$(DESTDIR)$(SYSTEMD_SYSTEM_DIR)/dynamic_power.service"
	@rm -vf "$(DESTDIR)$(DBUS_SYSTEM_POLICY_DIR)/org.dynamic_power.Daemon.conf"
	@rm -vf "$(DESTDIR)$(SHARE_DIR)"/dynamic-power*.yaml
	@rm -vf "$(DESTDIR)$(DESKTOP_DIR)/dynamic-power.desktop"
	@rm -vf "$(DESTDIR)$(PIXMAPS_DIR)/dynamic-power.svg"
	@echo "Uninstall complete."
