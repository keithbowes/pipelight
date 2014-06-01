PLUGIN_CONFIGS=$(wildcard share/configs/*.in)
PLUGIN_SCRIPTS=$(wildcard share/scripts/*.in)
PLUGIN_LICENSES=$(wildcard share/licenses/*.in)

include config.make

PROGRAMS := pluginloader32 winecheck32
ifeq ($(win64),true)
	PROGRAMS:= $(PROGRAMS) pluginloader64 winecheck64
endif

ifeq ($(debug),true)
	CXXFLAGS := $(CXXFLAGS) -DPIPELIGHT_DEBUG
endif

export

.PHONY: all
all: linux $(PROGRAMS)

.PHONY: linux
linux:
	$(MAKE) -C src/linux CXX="$(cxx)"

.PHONY: pluginloader32
pluginloader32:
	$(MAKE) -C src/windows wincxx="$(win32cxx)" winflags="$(win32flags)" suffix=""

.PHONY: pluginloader64
pluginloader64:
	$(MAKE) -C src/windows wincxx="$(win64cxx)" winflags="$(win64flags)" suffix="64"

.PHONY: winecheck32
winecheck32:
	$(MAKE) -C src/winecheck wincxx="$(win32cxx)" winflags="$(win32flags)" suffix=""

.PHONY: winecheck64
winecheck64:
	$(MAKE) -C src/winecheck wincxx="$(win64cxx)" winflags="$(win64flags)" suffix="64"

.PHONY: install
install: all
	mkdir -p "$(DESTDIR)$(prefix)/share/pipelight"
	mkdir -p "$(DESTDIR)$(prefix)/share/pipelight/configs"
	mkdir -p "$(DESTDIR)$(prefix)/share/pipelight/licenses"
	mkdir -p "$(DESTDIR)$(prefix)/share/pipelight/scripts"
	mkdir -p "$(DESTDIR)$(prefix)/lib/pipelight"
	mkdir -p "$(DESTDIR)$(prefix)/bin"
	mkdir -p "$(DESTDIR)$(prefix)/share/man/man1"
	mkdir -p "$(DESTDIR)$(mozpluginpath)"

	install -m 0644 share/sig-install-dependency.gpg "$(DESTDIR)$(prefix)/share/pipelight/sig-install-dependency.gpg"

	install -m 0755 "src/windows/pluginloader.exe" "$(DESTDIR)$(prefix)/share/pipelight/pluginloader.exe"
	if [ "$(win64)" = "true" ]; then \
		install -m 0755 "src/windows/pluginloader64.exe" "$(DESTDIR)$(prefix)/share/pipelight/pluginloader64.exe"; \
	fi

	install -m 0755 "src/winecheck/winecheck.exe" "$(DESTDIR)$(prefix)/share/pipelight/winecheck.exe"
	if [ "$(win64)" = "true" ]; then \
		install -m 0755 "src/winecheck/winecheck64.exe" "$(DESTDIR)$(prefix)/share/pipelight/winecheck64.exe"; \
	fi

	rm -f "$(DESTDIR)$(prefix)/share/pipelight/wine"
	ln -s "$(winepath)" "$(DESTDIR)$(prefix)/share/pipelight/wine"
	if [ "$(win64)" = "true" ]; then \
		rm -f "$(DESTDIR)$(prefix)/share/pipelight/wine64"; \
		ln -s "$(wine64path)" "$(DESTDIR)$(prefix)/share/pipelight/wine64"; \
	fi

	install -m 0755 share/install-dependency "$(DESTDIR)$(prefix)/share/pipelight/install-dependency"

	for script in $(notdir $(PLUGIN_SCRIPTS)); do \
		sed         's|@@PIPELIGHT_SHARE_PATH@@|$(prefix)/share/pipelight|g' share/scripts/$${script} > pipelight-script.tmp; \
		install -m 0755 pipelight-script.tmp "$(DESTDIR)$(prefix)/share/pipelight/scripts/$${script%.*}" || exit 1; \
		rm pipelight-script.tmp; \
	done

	for config in $(notdir $(PLUGIN_CONFIGS)); do \
		sed         's|@@GCC_RUNTIME_DLLS@@|$(gccruntimedlls)|g' share/configs/$${config} > pipelight-config.tmp; \
		sed -i'' -e 's|@@QUIET_INSTALLATION@@|$(quietinstallation)|g' pipelight-config.tmp; \
		install -m 0644 pipelight-config.tmp "$(DESTDIR)$(prefix)/share/pipelight/configs/$${config%.*}" || exit 1; \
		rm pipelight-config.tmp; \
	done

	for license in $(notdir $(PLUGIN_LICENSES)); do \
		sed    's|@@PIPELIGHT_SHARE_PATH@@|$(prefix)/share/pipelight|g' share/licenses/$${license} > pipelight-license.tmp; \
		install -m 0644 pipelight-license.tmp "$(DESTDIR)$(prefix)/share/pipelight/licenses/$${license%.*}" || exit 1; \
		rm pipelight-license.tmp; \
	done

	install -m 0644 src/linux/libpipelight.so "$(DESTDIR)$(prefix)/lib/pipelight/libpipelight.so"

	sed         's|@@VERSION@@|$(version)|g' bin/pipelight-plugin.in > pipelight-plugin.tmp
	sed -i'' -e 's|@@PIPELIGHT_SHARE_PATH@@|$(prefix)/share/pipelight|g' pipelight-plugin.tmp
	sed -i'' -e 's|@@PIPELIGHT_LIBRARY_PATH@@|$(prefix)/lib/pipelight/|g' pipelight-plugin.tmp
	sed -i'' -e 's|@@MOZ_PLUGIN_PATH@@|$(mozpluginpath)|g' pipelight-plugin.tmp
	install -m 0755 pipelight-plugin.tmp "$(DESTDIR)$(prefix)/bin/pipelight-plugin"
	rm pipelight-plugin.tmp

	sed         's|@@VERSION@@|$(version)|g' pipelight-plugin.1.in > pipelight-manpage.tmp
	sed -i'' -e 's|@@PREFIX@@|$(prefix)|g' pipelight-manpage.tmp
	install -m 0644 pipelight-manpage.tmp "$(DESTDIR)$(prefix)/share/man/man1/pipelight-plugin.1"
	rm pipelight-manpage.tmp

.PHONY: uninstall
uninstall:
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/sig-install-dependency.gpg"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/pluginloader.exe"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/pluginloader64.exe"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/winecheck.exe"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/winecheck64.exe"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/install-dependency"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/wine"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/wine64"
	rm -f  $(DESTDIR)$(prefix)/share/pipelight/scripts/configure-*
	rm -f  $(DESTDIR)$(prefix)/share/pipelight/configs/pipelight-*
	rm -f  $(DESTDIR)$(prefix)/share/pipelight/licenses/license-*
	rm -f "$(DESTDIR)$(prefix)/lib/pipelight/libpipelight.so"
	rm -f "$(DESTDIR)$(prefix)/bin/pipelight-plugin"
	rm -f "$(DESTDIR)$(prefix)/share/man/man1/pipelight-plugin.1"

	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(prefix)/share/pipelight/configs"
	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(prefix)/share/pipelight/licenses"
	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(prefix)/share/pipelight/scripts"
	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(prefix)/share/pipelight"
	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(prefix)/lib/pipelight"
	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(mozpluginpath)"

.PHONY: clean
clean:
	for dir in src/linux src/windows src/winecheck; do \
		$(MAKE) -C $$dir $@; \
	done
