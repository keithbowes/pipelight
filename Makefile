-include config.make

PLUGIN_CONFIGS=$(wildcard share/configs/*.in)
PLUGIN_SCRIPTS=$(wildcard share/scripts/*.in)
PLUGIN_LICENSES=$(wildcard share/licenses/*.in)

PROGRAMS := pluginloader32 winecheck32
ifeq ($(win64),true)
	PROGRAMS:= $(PROGRAMS) pluginloader64 winecheck64
endif

ifeq ($(debug),true)
	CXXFLAGS := $(CXXFLAGS) -DPIPELIGHT_DEBUG
endif

SED_OPTS :=	-e 's|@@BASH@@|$(bashinterp)|g' \
			-e '1s|/usr/bin/env bash|$(bashinterp)|' \
			-e 's|@@BINDIR@@|$(bindir)|g' \
			-e 's|@@DATADIR@@|$(datadir)|g' \
			-e 's|@@GCC_RUNTIME_DLLS@@|$(gccruntimedlls)|g' \
			-e 's|@@GPG@@|$(gpgexec)|g' \
			-e 's|@@LIBDIR@@|$(libdir)|g' \
			-e 's|@@MANDIR@@|$(mandir)|g' \
			-e 's|@@MOZ_PLUGIN_PATH@@|$(mozpluginpath)|g' \
			-e 's|@@PIPELIGHT_LIBRARY_PATH@@|$(libdir)/pipelight|g' \
			-e 's|@@PIPELIGHT_SHARE_PATH@@|$(datadir)/pipelight|g' \
			-e 's|@@PREFIX@@|$(prefix)|g' \
			-e 's|@@QUIET_INSTALLATION@@|$(quietinstallation)|g' \
			-e 's|@@VERSION@@|$(version)|g'

export

.PHONY: all
all: linux $(PROGRAMS)

config.make:
	@echo ""
	@echo "You need to call ./configure first." >&2
	@echo ""
	@exit 1

.PHONY: linux
linux: config.make
	$(MAKE) -C src/linux CXX="$(cxx)"

.PHONY: pluginloader32
pluginloader32: config.make
	$(MAKE) -C src/windows mingw_cxxflags="$(mingw_cxxflags)" wincxx="$(win32cxx)" winflags="$(win32flags)" suffix=""

.PHONY: pluginloader64
pluginloader64: config.make
	$(MAKE) -C src/windows mingw_cxxflags="$(mingw_cxxflags)" wincxx="$(win64cxx)" winflags="$(win64flags)" suffix="64"

.PHONY: winecheck32
winecheck32: config.make
	$(MAKE) -C src/winecheck mingw_cxxflags="$(mingw_cxxflags)" wincxx="$(win32cxx)" winflags="$(win32flags)" suffix=""

.PHONY: winecheck64
winecheck64: config.make
	$(MAKE) -C src/winecheck mingw_cxxflags="$(mingw_cxxflags)" wincxx="$(win64cxx)" winflags="$(win64flags)" suffix="64"

.PHONY: install
install: config.make all
	mkdir -p \
			"$(DESTDIR)$(bindir)" \
			"$(DESTDIR)$(datadir)/pipelight/configs" \
			"$(DESTDIR)$(datadir)/pipelight/licenses" \
			"$(DESTDIR)$(datadir)/pipelight/scripts" \
			"$(DESTDIR)$(libdir)/pipelight" \
			"$(DESTDIR)$(mandir)/man1" \
			"$(DESTDIR)$(mozpluginpath)"

	install -m 0644 share/sig-install-dependency.gpg "$(DESTDIR)$(datadir)/pipelight/sig-install-dependency.gpg"

	install -m 0755 "src/windows/pluginloader.exe" "$(DESTDIR)$(datadir)/pipelight/pluginloader.exe"
	if [ "$(win64)" = "true" ]; then \
		install -m 0755 "src/windows/pluginloader64.exe" "$(DESTDIR)$(datadir)/pipelight/pluginloader64.exe"; \
	fi

	install -m 0755 "src/winecheck/winecheck.exe" "$(DESTDIR)$(datadir)/pipelight/winecheck.exe"
	if [ "$(win64)" = "true" ]; then \
		install -m 0755 "src/winecheck/winecheck64.exe" "$(DESTDIR)$(datadir)/pipelight/winecheck64.exe"; \
	fi

	rm -f "$(DESTDIR)$(datadir)/pipelight/wine"
	ln -s "$(winepath)" "$(DESTDIR)$(datadir)/pipelight/wine"
	if [ "$(win64)" = "true" ]; then \
		rm -f "$(DESTDIR)$(datadir)/pipelight/wine64"; \
		ln -s "$(wine64path)" "$(DESTDIR)$(datadir)/pipelight/wine64"; \
	fi

	sed $(SED_OPTS) share/install-dependency > install-dependency.tmp
	install -m 0755 install-dependency.tmp "$(DESTDIR)$(datadir)/pipelight/install-dependency"
	rm install-dependency.tmp

	for script in $(notdir $(PLUGIN_SCRIPTS)); do \
		sed $(SED_OPTS) share/scripts/$${script} > pipelight-script.tmp; \
		install -m 0755 pipelight-script.tmp "$(DESTDIR)$(datadir)/pipelight/scripts/$${script%.*}" || exit 1; \
		rm pipelight-script.tmp; \
	done

	for config in $(notdir $(PLUGIN_CONFIGS)); do \
		sed $(SED_OPTS) share/configs/$${config} > pipelight-config.tmp; \
		install -m 0644 pipelight-config.tmp "$(DESTDIR)$(datadir)/pipelight/configs/$${config%.*}" || exit 1; \
		rm pipelight-config.tmp; \
	done

	for license in $(notdir $(PLUGIN_LICENSES)); do \
		sed $(SED_OPTS) share/licenses/$${license} > pipelight-license.tmp; \
		install -m 0644 pipelight-license.tmp "$(DESTDIR)$(datadir)/pipelight/licenses/$${license%.*}" || exit 1; \
		rm pipelight-license.tmp; \
	done

	install -m $(so_mode) src/linux/libpipelight.so "$(DESTDIR)$(libdir)/pipelight/libpipelight.so"

	sed $(SED_OPTS) bin/pipelight-plugin.in > pipelight-plugin.tmp
	install -m 0755 pipelight-plugin.tmp "$(DESTDIR)$(bindir)/pipelight-plugin"
	rm pipelight-plugin.tmp

	sed $(SED_OPTS) pipelight-plugin.1.in > pipelight-manpage.tmp
	install -m 0644 pipelight-manpage.tmp "$(DESTDIR)$(mandir)/man1/pipelight-plugin.1"
	rm pipelight-manpage.tmp

.PHONY: uninstall
uninstall: config.make
	rm -f	"$(DESTDIR)$(bindir)/pipelight-plugin" \
			 $(DESTDIR)$(datadir)/pipelight/configs/pipelight-* \
			"$(DESTDIR)$(datadir)/pipelight/install-dependency" \
			 $(DESTDIR)$(datadir)/pipelight/licenses/license-* \
			"$(DESTDIR)$(datadir)/pipelight/pluginloader.exe" \
			"$(DESTDIR)$(datadir)/pipelight/pluginloader64.exe" \
			 $(DESTDIR)$(datadir)/pipelight/scripts/configure-* \
			"$(DESTDIR)$(datadir)/pipelight/sig-install-dependency.gpg" \
			"$(DESTDIR)$(datadir)/pipelight/wine" \
			"$(DESTDIR)$(datadir)/pipelight/wine64" \
			"$(DESTDIR)$(datadir)/pipelight/winecheck.exe" \
			"$(DESTDIR)$(datadir)/pipelight/winecheck64.exe" \
			"$(DESTDIR)$(libdir)/pipelight/libpipelight.so" \
			"$(DESTDIR)$(mandir)/man1/pipelight-plugin.1"

	rmdir --ignore-fail-on-non-empty \
			"$(DESTDIR)$(datadir)/pipelight/configs" \
			"$(DESTDIR)$(datadir)/pipelight/licenses" \
			"$(DESTDIR)$(datadir)/pipelight/scripts" \
			"$(DESTDIR)$(datadir)/pipelight" \
			"$(DESTDIR)$(libdir)/pipelight" \
			"$(DESTDIR)$(mozpluginpath)"

.PHONY: clean
clean:
	for dir in src/linux src/windows src/winecheck; do \
		$(MAKE) -C $$dir $@; \
	done

.PHONY: dist-clean
dist-clean: clean
	rm -f config.make
