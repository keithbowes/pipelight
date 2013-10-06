SUBDIRS= src/linux src/windows
.PHONY:	all $(SUBDIRS) clean install uninstall

CONFIGS=$(wildcard configs/*)
prefix=/usr/local/
-include config.make

export
all: $(SUBDIRS)	

 $(SUBDIRS):
	$(MAKE) -C $@

install: all
	test -d "$(DESTDIR)$(prefix)/share/pipelight" || mkdir -p "$(DESTDIR)$(prefix)/share/pipelight"
	install -m 0644 src/windows/pluginloader.exe "$(DESTDIR)$(prefix)/share/pipelight/pluginloader.exe"
	
	@for config in $(notdir $(CONFIGS)) ; do \
		sed    's|PLUGIN_LOADER_PATH|$(prefix)/share/pipelight/pluginloader.exe|g' configs/$${config} > pipelight-config.tmp; \
		sed -i 's|DEPENDENCY_INSTALLER|$(prefix)/share/pipelight/install-dependency|g' pipelight-config.tmp; \
		install -m 0644 pipelight-config.tmp "$(DESTDIR)$(prefix)/share/pipelight/$${config}"; \
		rm pipelight-config.tmp; \
	done

	install -m 0755 misc/install-dependency "$(DESTDIR)$(prefix)/share/pipelight/install-dependency"

	test -d "$(DESTDIR)$(prefix)/bin/" || mkdir -p "$(DESTDIR)$(prefix)/bin/"
	sed 's|PLUGIN_SYSTEM_DIR|$(prefix)/lib/pipelight/|g' misc/pipelight-plugin > pipelight-plugin.tmp
	install -m 0755 pipelight-plugin.tmp "$(DESTDIR)$(prefix)/bin/pipelight-plugin"
	rm pipelight-plugin.tmp

	test -d "$(DESTDIR)$(prefix)/lib/pipelight" || mkdir -p "$(DESTDIR)$(prefix)/lib/pipelight"
	install -m 0644 src/linux/libpipelight.so "$(DESTDIR)$(prefix)/lib/pipelight/libpipelight.so"

uninstall:
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/pluginloader.exe"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/pipelight-*"
	rm -f "$(DESTDIR)$(prefix)/share/pipelight/install-dependency"
	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(prefix)/share/pipelight"
	rm -f "$(DESTDIR)$(prefix)/bin/pipelight-plugin"
	rm -f "$(DESTDIR)$(prefix)/lib/pipelight/libpipelight.so"
	rmdir --ignore-fail-on-non-empty "$(DESTDIR)$(prefix)/lib/pipelight"

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done