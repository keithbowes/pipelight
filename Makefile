PLUGIN_CONFIGS=$(wildcard share/configs/*)
PLUGIN_SCRIPTS=$(wildcard share/scripts/*)
PLUGIN_LICENSES=$(wildcard share/licenses/*)

version=unknown
prefix=/usr/local
winepath=/opt/wine-compholio/bin/wine
mozpluginpath=/usr/lib/mozilla/plugins
gccruntimedlls=
win32cxx=i686-w64-mingw32-g++
win32flags=-m32 -static-libgcc -static-libstdc++ -static
win64=false
win64cxx=
win64flags=-m64 -static-libgcc -static-libstdc++ -static
quietinstallation=true
nogpuaccel=false

-include config.make

ifeq ($(nogpuaccel),true)
	hwacceldefault=/bin/false
else
	hwacceldefault=$(prefix)/share/pipelight/hw-accel-default
endif

PLUGINLOADERS:= pluginloader32
ifeq ($(win64),true)
	PLUGINLOADERS:= $(PLUGINLOADERS) pluginloader64
endif

export

.PHONY: all
all: linux $(PLUGINLOADERS)

.PHONY: linux
linux:
	$(MAKE) -C src/linux

.PHONY: pluginloader32
pluginloader32:
	$(MAKE) -C src/windows wincxx="$(win32cxx)" winflags="$(win32flags)" suffix=""

.PHONY: pluginloader64
pluginloader64:
	$(MAKE) -C src/windows wincxx="$(win64cxx)" winflags="$(win64flags)" suffix="64"

.PHONY: install
install: all
	mkdir -p "$(DESTDIR)$(prefix)/share/pipelight"
	mkdir -p "$(DESTDIR)$(prefix)/share/pipelight/configs"
	mkdir -p "$(DESTDIR)$(prefix)/share/pipelight/licenses"
	mkdir -p "$(DESTDIR)$(prefix)/share/pipelight/scripts"
	mkdir -p "$(DESTDIR)$(prefix)/lib/pipelight"
	mkdir -p "$(DESTDIR)$(prefix)/bin"
	mkdir -p "$(DESTDIR)$(prefix)/share/man/man1"

	install -m 0644 share/sig-install-dependency.gpg "$(DESTDIR)$(prefix)/share/pipelight/sig-install-dependency.gpg"
	install -m 0755 "src/windows/pluginloader.exe" "$(DESTDIR)$(prefix)/share/pipelight/pluginloader.exe"
	if [ "$(win64)" = "true" ]; then \
		install -m 0755 "src/windows/pluginloader64.exe" "$(DESTDIR)$(prefix)/share/pipelight/pluginloader64.exe"; \
	fi
	install -m 0755 share/install-dependency "$(DESTDIR)$(prefix)/share/pipelight/install-dependency"
	install -m 0755 share/hw-accel-default "$(DESTDIR)$(prefix)/share/pipelight/hw-accel-default"

	for script in $(notdir $(PLUGIN_SCRIPTS)); do \
		sed         's|@@WINE_PATH@@|$(winepath)|g' share/scripts/$${script} > pipelight-script.tmp; \
		install -m 0755 pipelight-script.tmp "$(DESTDIR)$(prefix)/share/pipelight/scripts/$${script%.*}" || exit 1; \
		rm pipelight-script.tmp; \
	done

	for config in $(notdir $(PLUGIN_CONFIGS)); do \
		sed         's|@@PLUGINLOADER_PATH@@|$(prefix)/share/pipelight/pluginloader.exe|g' share/configs/$${config} > pipelight-config.tmp; \
		sed -i'' -e 's|@@PLUGINLOADER64_PATH@@|$(prefix)/share/pipelight/pluginloader64.exe|g' pipelight-config.tmp; \
		sed -i'' -e 's|@@DEPENDENCY_INSTALLER@@|$(prefix)/share/pipelight/install-dependency|g' pipelight-config.tmp; \
		sed -i'' -e 's|@@SANDBOX_PATH@@|$(prefix)/share/pipelight/sandbox|g' pipelight-config.tmp; \
		sed -i'' -e 's|@@GRAPHIC_DRIVER_CHECK@@|$(hwacceldefault)|g' pipelight-config.tmp; \
		sed -i'' -e 's|@@WINE_PATH@@|$(winepath)|g' pipelight-config.tmp; \
		sed -i'' -e 's|@@GCC_RUNTIME_DLLS@@|$(gccruntimedlls)|g' pipelight-config.tmp; \
		sed -i'' -e 's|@@QUIET_INSTALLATION@@|$(quietinstallation)|g' pipelight-config.tmp; \
		install -m 0644 pipelight-config.tmp "$(DESTDIR)$(prefix)/share/pipelight/configs/$${config%.*}" || exit 1; \
		rm pipelight-config.tmp; \
	done

	for license in $(notdir $(PLUGIN_LICENSES)); do \
		sed    's|@@LICENSE_PATH@@|$(prefix)/share/pipelight/licenses|g' share/licenses/$${license} > pipelight-license.tmp; \
		install -m 0644 pipelight-license.tmp "$(DESTDIR)$(prefix)/share/pipelight/licenses/$${license%.*}" || exit 1; \
		rm pipelight-license.tmp; \
	done

	install -m 0644 src/linux/libpipelight.so "$(DESTDIR)$(prefix)/lib/pipelight/libpipelight.so"

	sed         's|@@VERSION@@|$(version)|g' bin/pipelight-plugin > pipelight-plugin.tmp
	sed -i'' -e 's|@@LIBRARY_PATH@@|$(prefix)/lib/pipelight/|g' pipelight-plugin.tmp
	sed -i'' -e 's|@@CONFIG_PATH@@|$(prefix)/share/pipelight/configs|g' pipelight-plugin.tmp
	sed -i'' -e 's|@@LICENSE_PATH@@|$(prefix)/share/pipelight/licenses|g' pipelight-plugin.tmp
	sed -i'' -e 's|@@DEPENDENCY_INSTALLER@@|$(prefix)/share/pipelight/install-dependency|g' pipelight-plugin.tmp
	sed -i'' -e 's|@@MOZ_PLUGIN_PATH@@|$(mozpluginpath)|g' pipelight-plugin.tmp
	sed -i'' -e 's|@@PIPELIGHT_PUBKEY@@|$(prefix)/share/pipelight/sig-install-dependency.gpg|g' pipelight-plugin.tmp
	sed -i'' -e 's|@@PLUGINLOADER64_EXISTS@@|$(win64)|g' pipelight-plugin.tmp
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
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/install-dependency"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/hw-accel-default"
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

.PHONY: clean
clean:
	for dir in src/linux src/windows; do \
		$(MAKE) -C $$dir $@; \
	done