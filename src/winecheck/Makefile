suffix		:=
CXX			:= $(wincxx)
CXXFLAGS	:= -Wall -std=gnu++0x -D_WIN32_WINNT=0x0502 -DPLUGINLOADER $(winflags) $(mingw_cxxflags)
LDFLAGS		:= -lgdi32 -lopengl32
OBJECTS		:= check$(suffix).o

ifeq ($(wincxx),prebuilt)

.PHONY: all
all: winecheck$(suffix).exe

ifeq ($(wildcard winecheck$(suffix).exe),)
commit=$(shell git log --pretty=format:'%H' -n 1 || echo "prebuilt")

../../pluginloader-$(commit).tar.gz:
	$(downloader) "../../pluginloader-$(commit).tar.gz"     "http://repos.fds-team.de/pluginloader/$(commit)/pluginloader.tar.gz"

../../pluginloader-$(commit).tar.gz.sig:
	$(downloader) "../../pluginloader-$(commit).tar.gz.sig" "http://repos.fds-team.de/pluginloader/$(commit)/pluginloader.tar.gz.sig"

winecheck$(suffix).exe: ../../pluginloader-$(commit).tar.gz ../../pluginloader-$(commit).tar.gz.sig
	$(gpgexec) --batch --no-default-keyring --keyring "../../share/sig-pluginloader.gpg" --verify "../../pluginloader-$(commit).tar.gz.sig" && \
	tar --strip-components=2 -xvf "../../pluginloader-$(commit).tar.gz" src/winecheck/winecheck$(suffix).exe

else

winecheck$(suffix).exe:

endif

.PHONY: clean
clean:
	rm -f *.o

else

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

endif
