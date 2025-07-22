
# python/Makefile
# --------------------------------------------------------------------
# Installs the dynamic_power Python package + helper scripts
# --------------------------------------------------------------------
PREFIX        ?= /usr
BINDIR        ?= $(PREFIX)/bin
LIBDIR        ?= $(PREFIX)/lib
SYSTEMD_USER  ?= /usr/lib/systemd/user
PRESET_DIR    ?= /usr/lib/systemd/user-preset

DESTDIR       ?=

PY_PKG_DIR    = dynamic_power
RES_DIR       = resources
DBUS_DIR      = $(RES_DIR)/dbus
SYSD_USER_DIR = $(RES_DIR)/systemd-user
SHARE_DIR     = share/dynamic-power

.PHONY: all install uninstall

all:
	@echo "Nothing to build – pure Python package"

install:
	# --- Python package -------------------------------------------------
	install -d $(DESTDIR)$(LIBDIR)/$(PY_PKG_DIR)
	cp -r $(PY_PKG_DIR)/*.py $(DESTDIR)$(LIBDIR)/$(PY_PKG_DIR)/

	# --- User‑space executables ----------------------------------------
	install -Dm755 $(PY_PKG_DIR)/dynamic_power_user.py \
		$(DESTDIR)$(BINDIR)/dynamic_power_user
	install -Dm755 $(PY_PKG_DIR)/dynamic_power_command.py \
		$(DESTDIR)$(BINDIR)/dynamic_power_command
	install -Dm755 $(PY_PKG_DIR)/dynamic_power_session_helper.py \
		$(DESTDIR)$(BINDIR)/dynamic_power_session_helper

	# --- System daemon DBus policy (root) ------------------------------
	install -Dm644 $(DBUS_DIR)/org.dynamic_power.Daemon.conf \
		$(DESTDIR)/etc/dbus-1/system.d/org.dynamic_power.Daemon.conf

	# --- YAML templates -------------------------------------------------
	install -Dm644 $(SHARE_DIR)/dynamic-power.yaml \
		$(DESTDIR)$(PREFIX)/share/dynamic-power/dynamic-power.yaml
	install -Dm644 $(SHARE_DIR)/dynamic-power-user.yaml \
		$(DESTDIR)$(PREFIX)/share/dynamic-power/dynamic-power-user.yaml

	# --- User‑session systemd unit + preset ----------------------------
	install -Dm644 $(SYSD_USER_DIR)/dynamic_power_session.service \
		$(DESTDIR)$(SYSTEMD_USER)/dynamic_power_session.service
	install -Dm644 $(SYSD_USER_DIR)/90-dynamic-power.preset \
		$(DESTDIR)$(PRESET_DIR)/90-dynamic-power.preset

	@echo "Install complete."

uninstall:
	@echo "Removing installed files …"
	rm -f $(DESTDIR)$(BINDIR)/dynamic_power_user
	rm -f $(DESTDIR)$(BINDIR)/dynamic_power_command
	rm -f $(DESTDIR)$(BINDIR)/dynamic_power_session_helper
	rm -rf $(DESTDIR)$(LIBDIR)/$(PY_PKG_DIR)
	rm -f $(DESTDIR)/etc/dbus-1/system.d/org.dynamic_power.Daemon.conf
	rm -f $(DESTDIR)$(SYSTEMD_USER)/dynamic_power_session.service
	rm -f $(DESTDIR)$(PRESET_DIR)/90-dynamic-power.preset
	@echo "Uninstall complete."
