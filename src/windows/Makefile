
ifeq ($(USE_WINEGCC),1)
	CXX 	:= wineg++ -m32
else
	CXX 	:= i686-w64-mingw32-g++
endif

CXXFLAGS	:= -Wall -DXP_WIN=1 -std=gnu++0x -D_WIN32_WINNT=0x0502 $(win32flags) $(CXXFLAGS)
LDFLAGS 	:= -lversion -lgdi32 -lole32 #$(LDFLAGS)
OBJECTS 	:= common.o npclass.o npnfunctions.o apihook.o
COMMONHDR	:= ../common/common.h ../npapi-headers/npapi.h ../npapi-headers/npfunctions.h ../npapi-headers/npruntime.h ../npapi-headers/nptypes.h

pluginloader.exe: pluginloader.c pluginloader.h apihook.h $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) pluginloader.c $(OBJECTS) $(LDFLAGS) -o pluginloader.exe

common.o: ../common/common.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../common/common.c -o common.o

npclass.o: npclass.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npclass.c -o npclass.o

npnfunctions.o: npnfunctions.c pluginloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npnfunctions.c -o npnfunctions.o

apihook.o: apihook.c apihook.h pluginloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c apihook.c -o apihook.o

.PHONY: all
all: pluginloader.exe

.PHONY: clean
clean:
	rm -f pluginloader.exe *.o
