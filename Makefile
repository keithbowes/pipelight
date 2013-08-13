SUBDIRS= src/linux src/windows
.PHONY:	all $(SUBDIRS) clean install

prefix=/usr/local/

export
all: $(SUBDIRS)	

 $(SUBDIRS):
	$(MAKE) -C $@

install: all
	test -d $(DESTDIR)$(prefix)/share/pipelight || mkdir -p $(DESTDIR)$(prefix)/share/pipelight
	install -m 0644 src/windows/pluginloader.exe $(DESTDIR)$(prefix)/share/pipelight/pluginloader.exe
	sed 's|PLUGIN_LOADER_PATH|$(prefix)share/pipelight/pluginloader.exe|g' share/pipelight > pipelight.tmp
	install -m 0644 pipelight.tmp $(DESTDIR)$(prefix)/share/pipelight/pipelight
	rm pipelight.tmp
	test -d $(DESTDIR)/usr/lib/mozilla/plugins/ || mkdir -p $(DESTDIR)/usr/lib/mozilla/plugins/
	install -m 0644 src/linux/libpipelight.so $(DESTDIR)/usr/lib/mozilla/plugins/

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done