CXX			:= $(win32cxx)
CXXFLAGS	:= -Wall -DXP_WIN=1 -std=gnu++0x -D_WIN32_WINNT=0x0502 $(win32flags) $(CXXFLAGS)
LDFLAGS 	:= -lversion -lgdi32 -lole32 #$(LDFLAGS)
OBJECTS 	:= common.o apihook.o npclass.o npnfunctions.o pluginloader.o
COMMONHDR	:= ../common/common.h ../npapi-headers/npapi.h ../npapi-headers/npfunctions.h ../npapi-headers/npruntime.h ../npapi-headers/nptypes.h

ifeq ($(win32cxx),prebuilt)

pluginloader.exe:

.PHONY: all
all: pluginloader.exe

.PHONY: clean
clean:
	rm -f *.o

else

pluginloader.exe: $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(OBJECTS) $(LDFLAGS) -o pluginloader.exe

common.o: ../common/common.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../common/common.c -o common.o

apihook.o: apihook.c apihook.h pluginloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DPIPELIGHT_VERSION='"$(version)"' -c apihook.c -o apihook.o

npclass.o: npclass.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npclass.c -o npclass.o

npnfunctions.o: npnfunctions.c pluginloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npnfunctions.c -o npnfunctions.o

pluginloader.o: pluginloader.c pluginloader.h apihook.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c pluginloader.c -o pluginloader.o

.PHONY: all
all: pluginloader.exe

.PHONY: clean
clean:
	rm -f pluginloader.exe pluginloader.exe.so *.o

endif