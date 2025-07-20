PREFIX ?= /usr
DESTDIR ?=
SYSCONFDIR ?= /etc
DATADIR ?= $(PREFIX)/share
PYTHON_VERSION := $(shell python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
PYTHON_LIBDIR := $(PREFIX)/lib/python$(PYTHON_VERSION)/site-packages
DBUSDIR = /etc/dbus-1/system.d
DBUS_POLICY_SRC = resources/dbus/org.dynamic_power.Daemon.conf
DBUS_POLICY_DST = $(DBUSDIR)/org.dynamic_power.Daemon.conf

install:
	install -Dm755 dynamic_power/dynamic_power_user.py $(DESTDIR)/usr/bin/dynamic_power_user
	install -d $(DESTDIR)$(PYTHON_LIBDIR)/dynamic_power
	install -m 644 dynamic_power/*.py $(DESTDIR)$(PYTHON_LIBDIR)/dynamic_power/
	install -d $(DESTDIR)$(DATADIR)/dynamic-power
	install -m 644 share/dynamic-power/*.yaml $(DESTDIR)$(DATADIR)/dynamic-power/
	install -Dm644 $(DBUS_POLICY_SRC) $(DESTDIR)$(DBUS_POLICY_DST)

uninstall:
	rm -f $(DESTDIR)/usr/bin/dynamic_power_user
	install -Dm755 dynamic_power/dynamic_power_user.py $(DESTDIR)/usr/bin/dynamic_power_user
	rm -rf $(DESTDIR)$(PYTHON_LIBDIR)/dynamic_power
	rm -f $(DESTDIR)$(DATADIR)/dynamic-power/dynamic-power.yaml
	rm -f $(DESTDIR)$(DATADIR)/dynamic-power/dynamic-power-user.yaml
	rm -d $(DESTDIR)$(DATADIR)/dynamic-power 2>/dev/null || true
	rm -f $(DESTDIR)$(DBUS_POLICY_DST)