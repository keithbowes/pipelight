CXXFLAGS		:= -DPLUGINLOADER -DPIPELIGHT_VERSION='"$(version)"' $(CXXFLAGS)
LDFLAGS			:= -lversion -lgdi32 -lole32 -lopengl32
common_suffix 	:= win$(suffix)
export

SUBDIRS			:= ../../common/
SOURCE			= $(wildcard *.c)
OBJECTS			= $(SOURCE:.c=$(suffix).o) ../../common/common_win$(suffix).o

.PHONY: all
all: pluginloader$(suffix).exe

pluginloader$(suffix).exe: $(SUBDIRS) $(OBJECTS)
	rm -f "pluginloader$(suffix).exe.so"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) -o pluginloader$(suffix).exe

	if [ -f "pluginloader$(suffix).exe.so" ]; then \
		rm -f "pluginloader$(suffix).exe"; \
		mv "pluginloader$(suffix).exe.so" "pluginloader$(suffix).exe"; \
	fi

%$(suffix).o: %.c
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ -c $<

-include $(SOURCE:.c=.d)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

.PHONY: clean
clean:
	rm -f *.exe *.exe.so *.o *.d
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done