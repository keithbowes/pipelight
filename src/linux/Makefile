CXX 		:= g++
CXXFLAGS 	:= -Wall -DXP_UNIX=1 -DMOZ_X11=1 -fPIC -std=c++0x $(CXXFLAGS)
LDFLAGS 	:= -lX11 $(LDFLAGS)
OBJECTS 	:= communication.o handlemanager.o npclass.o configloader.o pluginfunctions.o

libpipelight.so: basicplugin.c basicplugin.h configloader.h ../npapi-headers/npapi.h ../npapi-headers/npruntime.h ../npapi-headers/npfunctions.h ../communication/communication.h ../handlemanager/handlemanager.h $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -z defs -shared basicplugin.c $(OBJECTS) $(LDFLAGS) -o libpipelight.so

communication.o: ../communication/communication.c ../communication/communication.h ../npapi-headers/npfunctions.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../communication/communication.c -o communication.o

handlemanager.o: ../handlemanager/handlemanager.c ../communication/communication.h ../handlemanager/handlemanager.h ../npapi-headers/npapi.h ../npapi-headers/npruntime.h ../npapi-headers/npfunctions.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../handlemanager/handlemanager.c -o handlemanager.o

npclass.o: npclass.c basicplugin.h ../npapi-headers/npapi.h ../npapi-headers/npruntime.h ../npapi-headers/npfunctions.h ../communication/communication.h ../handlemanager/handlemanager.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npclass.c -o npclass.o

configloader.o: configloader.c configloader.h basicplugin.h ../npapi-headers/npapi.h ../npapi-headers/npruntime.h ../npapi-headers/npfunctions.h ../communication/communication.h ../handlemanager/handlemanager.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DPREFIX='"$(prefix)"' -c configloader.c -o configloader.o

pluginfunctions.o: pluginfunctions.c basicplugin.h configloader.h ../npapi-headers/npapi.h ../npapi-headers/npruntime.h ../npapi-headers/npfunctions.h ../communication/communication.h ../handlemanager/handlemanager.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c pluginfunctions.c -o pluginfunctions.o

.PHONY: all
all: libpipelight.so

.PHONY: clean
clean:
	rm -f libpipelight.so *.o
