CXXFLAGS	:= -I../../../include -Wall -std=gnu++0x -fPIC $(CXXFLAGS)
export

SUBDIRS		= $(dir $(wildcard */Makefile))

.PHONY: all
all: $(SUBDIRS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

.PHONY: clean
clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done