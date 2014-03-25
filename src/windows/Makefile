suffix		:=
CXX			:= $(wincxx)
CXXFLAGS	:= -Wall -std=gnu++0x -D_WIN32_WINNT=0x0502 -DPLUGINLOADER $(winflags) $(CXXFLAGS)
LDFLAGS		:= -lversion -lgdi32 -lole32 #$(LDFLAGS)
OBJECTS		:= common$(suffix).o apihook$(suffix).o npclass$(suffix).o npnfunctions$(suffix).o pluginloader$(suffix).o
COMMONHDR	:= ../common/common.h ../npapi-headers/npapi.h ../npapi-headers/npfunctions.h ../npapi-headers/npruntime.h ../npapi-headers/nptypes.h

ifeq ($(wincxx),prebuilt)

.PHONY: all
all: pluginloader$(suffix).exe

ifeq ($(wildcard pluginloader$(suffix).exe),)
commit=$(shell git log --pretty=format:'%H' -n 1 || echo "prebuilt")

../../pluginloader-$(commit).tar.gz:
	wget -O "../../pluginloader-$(commit).tar.gz"     "http://repos.fds-team.de/pluginloader/$(commit)/pluginloader.tar.gz"

../../pluginloader-$(commit).tar.gz.sig:
	wget -O "../../pluginloader-$(commit).tar.gz.sig" "http://repos.fds-team.de/pluginloader/$(commit)/pluginloader.tar.gz.sig"

pluginloader$(suffix).exe: ../../pluginloader-$(commit).tar.gz ../../pluginloader-$(commit).tar.gz.sig
	gpg --batch --no-default-keyring --keyring "../../share/sig-pluginloader.gpg" --verify "../../pluginloader-$(commit).tar.gz.sig" && \
	tar -xvf "../../pluginloader-$(commit).tar.gz" src/windows/pluginloader$(suffix).exe --strip-components=2

else

pluginloader$(suffix).exe:

endif

.PHONY: clean
clean:
	rm -f *.o

else

.PHONY: all
all: pluginloader$(suffix).exe

pluginloader$(suffix).exe: $(OBJECTS)
	rm -f "pluginloader$(suffix).exe.so"

	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) -o pluginloader$(suffix).exe

	if [ -f "pluginloader$(suffix).exe.so" ]; then \
		rm -f "pluginloader$(suffix).exe"; \
		mv "pluginloader$(suffix).exe.so" "pluginloader$(suffix).exe"; \
	fi

common$(suffix).o: ../common/common.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../common/common.c -o common$(suffix).o

apihook$(suffix).o: apihook.c apihook.h pluginloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DPIPELIGHT_VERSION='"$(version)"' -c apihook.c -o apihook$(suffix).o

npclass$(suffix).o: npclass.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npclass.c -o npclass$(suffix).o

npnfunctions$(suffix).o: npnfunctions.c pluginloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npnfunctions.c -o npnfunctions$(suffix).o

pluginloader$(suffix).o: pluginloader.c pluginloader.h apihook.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c pluginloader.c -o pluginloader$(suffix).o

.PHONY: clean
clean:
	rm -f *.exe *.exe.so *.o

endif