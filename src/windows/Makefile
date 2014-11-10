CXXFLAGS	:= -I../../../include -Wall -std=gnu++0x -D_WIN32_WINNT=0x0502 $(CXXFLAGS)
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