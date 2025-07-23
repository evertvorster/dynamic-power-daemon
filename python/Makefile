# Makefile for dynamic-power-daemon
# ---------------------------------

PYTHON := python3
PYTHON_SITE := $(shell $(PYTHON) -c "import site; print(site.getsitepackages()[0])")

PREFIX ?= /usr
BINDIR := $(PREFIX)/bin

.PHONY: all install uninstall

all:
	@echo "Use 'make install' to install the dynamic-power-daemon"

install:
	# --- Python package ---
	install -d $(DESTDIR)$(PYTHON_SITE)/dynamic_power
	cp -r dynamic_power/*.py $(DESTDIR)$(PYTHON_SITE)/dynamic_power/

	# --- Executables ---
	install -Dm755 dynamic_power/dynamic_power_user.py \
		$(DESTDIR)$(BINDIR)/dynamic_power_user
	install -Dm755 dynamic_power/dynamic_power_command.py \
		$(DESTDIR)$(BINDIR)/dynamic_power_command
	install -Dm755 dynamic_power/dynamic_power_session_helper.py \
		$(DESTDIR)$(BINDIR)/dynamic_power_session_helper

	# --- DBus policy ---
	install -Dm644 resources/dbus/org.dynamic_power.Daemon.conf \
		$(DESTDIR)/etc/dbus-1/system.d/org.dynamic_power.Daemon.conf

	# --- YAML templates ---
	install -Dm644 share/dynamic-power/dynamic-power.yaml \
		$(DESTDIR)$(PREFIX)/share/dynamic-power/dynamic-power.yaml
	install -Dm644 share/dynamic-power/dynamic-power-user.yaml \
		$(DESTDIR)$(PREFIX)/share/dynamic-power/dynamic-power-user.yaml

	# --- systemd user units ---
	install -Dm644 resources/systemd-user/dynamic_power_session.service \
		$(DESTDIR)$(PREFIX)/lib/systemd/user/dynamic_power_session.service
	install -Dm644 resources/systemd-user/dynamic_power_command.service \
		$(DESTDIR)$(PREFIX)/lib/systemd/user/dynamic_power_command.service
	install -Dm644 resources/systemd-user/90-dynamic-power.preset \
		$(DESTDIR)$(PREFIX)/lib/systemd/user-preset/90-dynamic-power.preset

uninstall:
	rm -f $(BINDIR)/dynamic_power_user
	rm -f $(BINDIR)/dynamic_power_command
	rm -f $(BINDIR)/dynamic_power_session_helper
	rm -rf $(PYTHON_SITE)/dynamic_power
	rm -f /etc/dbus-1/system.d/org.dynamic_power.Daemon.conf
	rm -f $(PREFIX)/share/dynamic-power/dynamic-power.yaml
	rm -f $(PREFIX)/share/dynamic-power/dynamic-power-user.yaml
	rm -f $(PREFIX)/lib/systemd/user/dynamic_power_session.service
	rm -f $(PREFIX)/lib/systemd/user/dynamic_power_command.service
	rm -f $(PREFIX)/lib/systemd/user-preset/90-dynamic-power.preset
