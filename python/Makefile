# --- configurable paths ----------------------------------------------------
DESTDIR        ?=
BINDIR         ?= /usr/bin
SYSCONFDIR     ?= /etc
DATADIR        ?= /usr/share
PYTHON         ?= python3
INSTALL        ?= install
MKDIR_P        ?= mkdir -p
PYTHON_SITELIB ?= $(shell $(PYTHON) -c "import site, sys; print(site.getsitepackages()[0])")

# ---------------------------------------------------------------------------
all:
	@echo "Nothing to build.  Run 'make install'."

install:
	@echo "Installing dynamic_power stub …"

	# 1. Install CLI entry point
	$(MKDIR_P) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 dynamic_power/cli.py $(DESTDIR)$(BINDIR)/dynamic_power

	# 2. Install Python package
	$(MKDIR_P) $(DESTDIR)$(PYTHON_SITELIB)/dynamic_power
	cp dynamic_power/*.py $(DESTDIR)$(PYTHON_SITELIB)/dynamic_power/

	# 3. Install default YAML template to /usr/share
	$(MKDIR_P) $(DESTDIR)$(DATADIR)/dynamic-power
	$(INSTALL) -m 644 share/dynamic-power/dynamic-power.yaml \
		$(DESTDIR)$(DATADIR)/dynamic-power/dynamic-power.yaml

uninstall:
	rm -f  $(DESTDIR)$(BINDIR)/dynamic_power
	rm -rf $(DESTDIR)$(PYTHON_SITELIB)/dynamic_power
	rm -f  $(DESTDIR)$(DATADIR)/dynamic-power/dynamic-power.yaml

.PHONY: all install uninstall
