# Dynamic‑Power‑Daemon – project‑root Makefile
#
PYTHON  ?= python
PREFIX  ?= /usr
DESTDIR ?=

SRC_DIR          := python
MODULE_DIR       := dynamic_power
CPP_SRC_DIR		 := src
BUILD_DIR		 := build
RESOURCE_DIR     := resources

PURELIB := $(shell $(PYTHON) -c "import sysconfig, sys; print(sysconfig.get_paths()['purelib'])")

BINDIR                 := $(PREFIX)/bin
SYSTEMD_SYSTEM_DIR     := $(PREFIX)/lib/systemd/system
SYSTEMD_USER_DIR       := $(PREFIX)/lib/systemd/user
SYSTEMD_USER_PRESET    := $(PREFIX)/lib/systemd/user-preset
DBUS_SYSTEM_POLICY_DIR := /etc/dbus-1/system.d
SHARE_DIR              := /usr/share/dynamic-power
DESKTOP_DIR            := $(PREFIX)/share/applications
PIXMAPS_DIR 	       := $(PREFIX)/share/icons/hicolor/scalable/apps/

.PHONY: all install uninstall

all:
	@echo "Nothing to build. Use 'make install'."

install:
	@echo "# --- Python package -------------------------------------------------"
	install -d "$(DESTDIR)$(PURELIB)"
	cp -r $(SRC_DIR)/$(MODULE_DIR) "$(DESTDIR)$(PURELIB)/"

	@echo "# --- Executables ----------------------------------------------------"
	install -Dm755 $(CPP_SRC_DIR)/$(BUILD_DIR)/dynamic_power 		 "$(DESTDIR)$(BINDIR)/dynamic_power"
	install -Dm755 $(CPP_SRC_DIR)/$(BUILD_DIR)/dynamic_power_user         "$(DESTDIR)$(BINDIR)/dynamic_power_user"

	@echo "# --- System daemon unit -------------------------------------------"
	install -Dm644 dynamic-power.service                                 "$(DESTDIR)$(SYSTEMD_SYSTEM_DIR)/dynamic_power.service"

	@echo "# --- DBus policy ---------------------------------------------------"
	install -Dm644 $(RESOURCE_DIR)/dbus/org.dynamic_power.Daemon.conf \
		"$(DESTDIR)$(DBUS_SYSTEM_POLICY_DIR)/org.dynamic_power.Daemon.conf"

	@echo "# --- Desktop entry --------------------------------------------------"
	install -Dm644 $(RESOURCE_DIR)/dynamic-power.desktop \
		"$(DESTDIR)$(DESKTOP_DIR)/dynamic-power.desktop"

	@echo "# --- Icon -----------------------------------------------------------"
	install -Dm644 $(RESOURCE_DIR)/dynamic_power.default_ac.svg "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.default_ac.svg"
	install -Dm644 $(RESOURCE_DIR)/dynamic_power.default_battery.svg "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.default_battery.svg"
	install -Dm644 $(RESOURCE_DIR)/dynamic_power.match_ac.svg "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.match_ac.svg"
	install -Dm644 $(RESOURCE_DIR)/dynamic_power.match_battery.svg "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.match_battery.svg"
	install -Dm644 $(RESOURCE_DIR)/dynamic_power.override_ac.svg "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.override_ac.svg"
	install -Dm644 $(RESOURCE_DIR)/dynamic_power.override_battery.svg "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.override_battery.svg"

	@echo "# --- YAML templates ------------------------------------------------"
	install -Dm644 $(RESOURCE_DIR)/dynamic-power-user.yaml "$(DESTDIR)$(SHARE_DIR)/dynamic-power-user.yaml"
	install -Dm644 $(RESOURCE_DIR)/dynamic_power.yaml      "$(DESTDIR)$(SHARE_DIR)/dynamic_power.yaml"

	@echo "Install complete."

uninstall:
	@echo "Removing installation (PREFIX=$(PREFIX))"
	@rm -vf "$(DESTDIR)$(BINDIR)"/dynamic_power*
	@rm -vrf "$(DESTDIR)$(PURELIB)/$(MODULE_DIR)"
	@rm -vf "$(DESTDIR)$(SYSTEMD_SYSTEM_DIR)/dynamic_power.service"
	@rm -vf "$(DESTDIR)$(DBUS_SYSTEM_POLICY_DIR)/org.dynamic_power.Daemon.conf"
	@rm -vf "$(DESTDIR)$(SHARE_DIR)"/dynamic-power*.yaml
	@rm -vf "$(DESTDIR)$(DESKTOP_DIR)/dynamic-power.desktop"
	@rm -vf "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.default_ac.svg"
	@rm -vf "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.default_battery.svg"
	@rm -vf "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.match_ac.svg"
	@rm -vf "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.match_battery.svg"
	@rm -vf "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.override_ac.svg"
	@rm -vf "$(DESTDIR)$(PIXMAPS_DIR)/dynamic_power.override_battery.svg"
	@echo "Uninstall complete."
