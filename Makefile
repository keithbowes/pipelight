SUBDIRS= src/linux src/windows
.PHONY:	all $(SUBDIRS) clean install uninstall

CONFIGS=$(wildcard configs/*)

version=unknown
prefix=/usr/local/
winepath=/opt/wine-compholio/bin/wine
mozpluginpath=/usr/lib/mozilla/plugins
gccruntimedlls=/usr/lib/gcc/i686-w64-mingw32/4.6/
quietinstallation=true
win32flags=

-include config.make

export
all: $(SUBDIRS)	

 $(SUBDIRS):
	$(MAKE) -C $@

install: all
	test -d "$(DESTDIR)$(prefix)/share/pipelight" || mkdir -p "$(DESTDIR)$(prefix)/share/pipelight"
	install -m 0644 src/windows/pluginloader.exe "$(DESTDIR)$(prefix)/share/pipelight/pluginloader.exe"
	
	for config in $(notdir $(CONFIGS)) ; do \
		sed    's|@@PLUGIN_LOADER_PATH@@|$(prefix)/share/pipelight/pluginloader.exe|g' configs/$${config} > pipelight-config.tmp; \
		sed -i 's|@@DEPENDENCY_INSTALLER@@|$(prefix)/share/pipelight/install-dependency|g' pipelight-config.tmp; \
		sed -i 's|@@GRAPHIC_DRIVER_CHECK@@|$(prefix)/share/pipelight/hw-accel-default|g' pipelight-config.tmp; \
		sed -i 's|@@WINE_PATH@@|$(winepath)|g' pipelight-config.tmp; \
		sed -i 's|@@GCC_RUNTIME_DLLS@@|$(gccruntimedlls)|g' pipelight-config.tmp; \
		sed -i 's|@@QUIET_INSTALLATION@@|$(quietinstallation)|g' pipelight-config.tmp; \
		install -m 0644 pipelight-config.tmp "$(DESTDIR)$(prefix)/share/pipelight/$${config}"; \
		rm pipelight-config.tmp; \
	done

	install -m 0755 misc/install-dependency "$(DESTDIR)$(prefix)/share/pipelight/install-dependency"
	install -m 0755 misc/hw-accel-default "$(DESTDIR)$(prefix)/share/pipelight/hw-accel-default"

	test -d "$(DESTDIR)$(prefix)/bin/" || mkdir -p "$(DESTDIR)$(prefix)/bin/"
	sed    's|@@PLUGIN_SYSTEM_PATH@@|$(prefix)/lib/pipelight/|g' misc/pipelight-plugin > pipelight-plugin.tmp
	sed -i 's|@@MOZ_PLUGIN_PATH@@|$(mozpluginpath)|g' pipelight-plugin.tmp
	install -m 0755 pipelight-plugin.tmp "$(DESTDIR)$(prefix)/bin/pipelight-plugin"
	rm pipelight-plugin.tmp

	test -d "$(DESTDIR)$(prefix)/lib/pipelight" || mkdir -p "$(DESTDIR)$(prefix)/lib/pipelight"
	install -m 0644 src/linux/libpipelight.so "$(DESTDIR)$(prefix)/lib/pipelight/libpipelight.so"

uninstall:
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/pluginloader.exe"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/pipelight-*"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/install-dependency"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/hw-accel-default"
	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(prefix)/share/pipelight"

	rm -f "$(DESTDIR)$(prefix)/bin/pipelight-plugin"

	rm -f "$(DESTDIR)$(prefix)/lib/pipelight/libpipelight.so"
	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(prefix)/lib/pipelight"

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done