include ../../../config.make
winelibdir = $(dir $(wine_path))../lib
CXXFLAGS		:= -DPLUGINLOADER -DPIPELIGHT_VERSION='"$(version)"' $(CXXFLAGS)
LDFLAGS			:= -lversion -lgdi32 -lole32 -lopengl32 -L$(winelibdir) -L$(winelibdir)/wine
common_suffix 	:= win$(suffix)
export

SUBDIRS			:= ../../common/
SOURCE			= $(wildcard *.c)
OBJECTS			= $(SOURCE:.c=$(suffix).o) ../../common/common_win$(suffix).o

.PHONY: all
all: pluginloader$(suffix).exe

pluginloader$(suffix).exe: apihook.h $(OBJECTS)
	rm -f "pluginloader$(suffix).exe.so"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) -o pluginloader$(suffix).exe

	if [ -f "pluginloader$(suffix).exe.so" ]; then \
		rm -f "pluginloader$(suffix).exe"; \
		mv "pluginloader$(suffix).exe.so" "pluginloader$(suffix).exe"; \
	fi

apihook.h: apihook.h.in
	sed $(SED_OPTS) $< > $@

%$(suffix).o: %.c
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ -c $<

../../common/common_win$(suffix).o: dummy
	$(MAKE) -C ../../common/

-include $(SOURCE:.c=.d)

.PHONY: clean dummy
clean:
	rm -f apihook.h *.exe *.exe.so *.o *.d
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
