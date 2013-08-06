SUBDIRS= linux windows
.PHONY:	all $(SUBDIRS) clean

all: $(SUBDIRS)	

 $(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done