include ../../../config.make
winelibdir = $(dir $(wine_path))../lib
LDFLAGS		:= -lgdi32 -lopengl32 -L$(winelibdir) -L$(winelibdir)/wine
CXXFLAGS = -m32
export

SOURCE		= $(wildcard *.c)
OBJECTS		= $(SOURCE:.c=$(suffix).o)

.PHONY: all
all: winecheck$(suffix).exe

winecheck$(suffix).exe: $(OBJECTS)
	rm -f "winecheck$(suffix).exe.so"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) -o winecheck$(suffix).exe

	if [ -f "winecheck$(suffix).exe.so" ]; then \
		rm -f "winecheck$(suffix).exe"; \
		mv "winecheck$(suffix).exe.so" "winecheck$(suffix).exe"; \
	fi

%$(suffix).o: %.c
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ -c $<

-include $(SOURCE:.c=.d)

.PHONY: clean
clean:
	rm -f *.exe *.exe.so *.o
