SUBDIRS= linux windows
.PHONY:	all $(SUBDIRS) clean install

prefix=/usr/local

all: $(SUBDIRS)	

 $(SUBDIRS):
	$(MAKE) -C $@

install: all
	test -d $(DESTDIR)$(prefix)/share/pipelight || mkdir -p $(DESTDIR)$(prefix)/share/pipelight
	install -m 0644 windows/pluginloader.exe $(DESTDIR)$(prefix)/share/pipelight/pluginloader.exe
	test -d $(DESTDIR)/usr/lib/mozilla/plugins/ || mkdir -p $(DESTDIR)/usr/lib/mozilla/plugins/
	install -m 0644 linux/pipelight.so $(DESTDIR)/usr/lib/mozilla/plugins/

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done