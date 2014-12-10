CXXFLAGS		:= -fvisibility=hidden -DPIPELIGHT_SHARE_PATH='"$(datadir)/pipelight"' $(CXXFLAGS)
LDFLAGS			:= -lpthread $(LDFLAGS)
UNAME_S			:= $(shell uname -s)
ifeq ($(UNAME_S), FreeBSD)
	CXXFLAGS	:= -I/usr/local/include $(CXXFLAGS)
	LDFLAGS		:= -L/usr/local/lib $(LDFLAGS)
else
	LDFLAGS		:= -ldl $(LDFLAGS)
endif
ifneq ($(UNAME_S),Darwin)
	LDFLAGS		:= -lX11 $(LDFLAGS)
endif
common_suffix	:= lin
export

SOURCE		= $(wildcard *.c)
OBJECTS		= $(SOURCE:.c=.o) ../../common/common_lin.o
SUBDIRS		:= ../../common/

.PHONY: all
all: libpipelight.so

libpipelight.so: $(OBJECTS)
	$(CXX) -shared $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) -o libpipelight.so

%.o: %.c
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ -c $<

../../common/common_lin.o: dummy
	$(MAKE) -C ../../common/

-include $(SOURCE:.c=.d)

.PHONY: clean dummy
clean:
	rm -f *.so *.o *.d
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done