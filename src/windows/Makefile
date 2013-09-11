
ifeq ($(USE_WINE_GCC),1)
	CXX 	:= wineg++ -m32
else
	CXX 	:= i686-w64-mingw32-g++
endif

CXXFLAGS	:= -Wall -DXP_WIN=1 -std=gnu++0x -D_WIN32_WINNT=0x0502 $(CXXFLAGS)
LDFLAGS 	:= -lversion -lgdi32 -lole32 #$(LDFLAGS)
OBJECTS 	:= communication.o handlemanager.o npclass.o npnfunctions.o apihook.o

pluginloader.exe: pluginloader.c pluginloader.h apihook.h $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) pluginloader.c $(OBJECTS) $(LDFLAGS) -o pluginloader.exe

communication.o: ../communication/communication.c ../communication/communication.h ../handlemanager/handlemanager.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../communication/communication.c -o communication.o

handlemanager.o: ../handlemanager/handlemanager.c ../communication/communication.h ../handlemanager/handlemanager.h ../npapi-headers/npapi.h ../npapi-headers/npruntime.h ../npapi-headers/npfunctions.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../handlemanager/handlemanager.c -o handlemanager.o

npclass.o: npclass.c ../npapi-headers/npapi.h ../npapi-headers/npruntime.h ../communication/communication.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npclass.c -o npclass.o

npnfunctions.o: npnfunctions.c pluginloader.h ../npapi-headers/npapi.h ../npapi-headers/npruntime.h ../npapi-headers/npfunctions.h ../communication/communication.h ../handlemanager/handlemanager.h ../npapi-headers/npapi.h ../npapi-headers/npruntime.h ../npapi-headers/npfunctions.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npnfunctions.c -o npnfunctions.o

apihook.o: apihook.c apihook.h ../communication/communication.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c apihook.c -o apihook.o

.PHONY: all
all: pluginloader.exe

.PHONY: clean
clean:
	rm -f pluginloader.exe *.o
