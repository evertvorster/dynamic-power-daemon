
PREFIX ?= /usr
DESTDIR ?=
SYSCONFDIR ?= /etc
DATADIR ?= $(PREFIX)/share
PYTHON_VERSION := $(shell python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
PYTHON_LIBDIR := $(PREFIX)/lib/python$(PYTHON_VERSION)/site-packages

install:
	install -d $(DESTDIR)$(PYTHON_LIBDIR)/dynamic_power
	install -m 644 dynamic_power/*.py $(DESTDIR)$(PYTHON_LIBDIR)/dynamic_power/
	install -d $(DESTDIR)$(DATADIR)/dynamic-power
	install -m 644 share/dynamic-power/*.yaml $(DESTDIR)$(DATADIR)/dynamic-power/

uninstall:
	rm -rf $(DESTDIR)$(PYTHON_LIBDIR)/dynamic_power
	rm -f $(DESTDIR)$(DATADIR)/dynamic-power/dynamic-power.yaml
	rm -f $(DESTDIR)$(DATADIR)/dynamic-power/dynamic-power-user.yaml
	rm -d $(DESTDIR)$(DATADIR)/dynamic-power 2>/dev/null || true