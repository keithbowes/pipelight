suffix		:=
CXX			:= $(wincxx)
CXXFLAGS	:= -Wall -std=gnu++0x -D_WIN32_WINNT=0x0502 -DPLUGINLOADER $(winflags) $(CXXFLAGS)
LDFLAGS		:= -lgdi32 -lopengl32
OBJECTS		:= check$(suffix).o

.PHONY: all
all: winecheck$(suffix).exe

winecheck$(suffix).exe: $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) -o winecheck$(suffix).exe

	if [ -f "winecheck$(suffix).exe.so" ]; then \
		rm -f "winecheck$(suffix).exe"; \
		mv "winecheck$(suffix).exe.so" "winecheck$(suffix).exe"; \
	fi

check$(suffix).o: check.c
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c check.c -o check$(suffix).o

.PHONY: clean
clean:
	rm -f *.exe *.exe.so *.o
