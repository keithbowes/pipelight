SUBDIRS= src/linux src/windows
.PHONY:	all $(SUBDIRS) clean install uninstall

PLUGIN_CONFIGS=$(wildcard plugin-configs/*)
PLUGIN_SCRIPTS=$(wildcard plugin-scripts/*)

version=unknown
prefix=/usr/local/
mozpluginpath=/usr/lib/mozilla/plugins
gccruntimedlls=/usr/lib/gcc/i686-w64-mingw32/4.6/
winepath=/opt/wine-compholio/bin/wine
quietinstallation=true
win32cxx=i686-w64-mingw32-g++
win32flags=
pluginloader=pluginloader.exe
nogpuaccel=false

-include config.make

ifeq ($(nogpuaccel),true)
	hwacceldefault=/bin/false
else
	hwacceldefault=$(prefix)/share/pipelight/hw-accel-default
endif


export
all: $(SUBDIRS)

 $(SUBDIRS):
	$(MAKE) -C $@

install: all
	test -d "$(DESTDIR)$(prefix)/share/pipelight" || mkdir -p "$(DESTDIR)$(prefix)/share/pipelight"
	test -d "$(DESTDIR)$(prefix)/bin/" || mkdir -p "$(DESTDIR)$(prefix)/bin/"
	test -d "$(DESTDIR)$(prefix)/lib/pipelight" || mkdir -p "$(DESTDIR)$(prefix)/lib/pipelight"

	install -m 0644 "src/windows/$(pluginloader)" "$(DESTDIR)$(prefix)/share/pipelight/$(pluginloader)"

	for config in $(notdir $(PLUGIN_CONFIGS)); do \
		sed    's|@@PLUGIN_LOADER_PATH@@|$(prefix)/share/pipelight/$(pluginloader)|g' plugin-configs/$${config} > pipelight-config.tmp; \
		sed -i 's|@@DEPENDENCY_INSTALLER@@|$(prefix)/share/pipelight/install-dependency|g' pipelight-config.tmp; \
		sed -i 's|@@GRAPHIC_DRIVER_CHECK@@|$(hwacceldefault)|g' pipelight-config.tmp; \
		sed -i 's|@@WINE_PATH@@|$(winepath)|g' pipelight-config.tmp; \
		sed -i 's|@@GCC_RUNTIME_DLLS@@|$(gccruntimedlls)|g' pipelight-config.tmp; \
		sed -i 's|@@QUIET_INSTALLATION@@|$(quietinstallation)|g' pipelight-config.tmp; \
		install -m 0644 pipelight-config.tmp "$(DESTDIR)$(prefix)/share/pipelight/$${config}"; \
		rm pipelight-config.tmp; \
	done

	for script in $(notdir $(PLUGIN_SCRIPTS)); do \
		sed    's|@@WINE_PATH@@|$(winepath)|g' plugin-scripts/$${script} > pipelight-script.tmp; \
		install -m 0755 pipelight-script.tmp "$(DESTDIR)$(prefix)/share/pipelight/$${script}"; \
		rm pipelight-script.tmp; \
	done

	install -m 0755 internal-scripts/install-dependency "$(DESTDIR)$(prefix)/share/pipelight/install-dependency"
	install -m 0755 internal-scripts/hw-accel-default "$(DESTDIR)$(prefix)/share/pipelight/hw-accel-default"

	sed    's|@@PLUGIN_SYSTEM_PATH@@|$(prefix)/lib/pipelight/|g' public-scripts/pipelight-plugin > pipelight-plugin.tmp
	sed -i 's|@@DEPENDENCY_INSTALLER@@|$(prefix)/share/pipelight/install-dependency|g' pipelight-plugin.tmp
	sed -i 's|@@MOZ_PLUGIN_PATH@@|$(mozpluginpath)|g' pipelight-plugin.tmp
	install -m 0755 pipelight-plugin.tmp "$(DESTDIR)$(prefix)/bin/pipelight-plugin"
	rm pipelight-plugin.tmp

	install -m 0644 src/linux/libpipelight.so "$(DESTDIR)$(prefix)/lib/pipelight/libpipelight.so"

uninstall:
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/$(pluginloader)"
	rm -f  $(DESTDIR)$(prefix)/share/pipelight/pipelight-*
	rm -f  $(DESTDIR)$(prefix)/share/pipelight/configure-*
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/install-dependency"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/hw-accel-default"
	rm -f "$(DESTDIR)$(prefix)/bin/pipelight-plugin"
	rm -f "$(DESTDIR)$(prefix)/lib/pipelight/libpipelight.so"

	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(prefix)/share/pipelight"
	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(prefix)/lib/pipelight"

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done