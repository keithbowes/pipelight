CXX 		:= g++
CXXFLAGS 	:= -Wall -DXP_UNIX=1 -DMOZ_X11=1 -fPIC -std=gnu++0x $(CXXFLAGS)
LDFLAGS 	:= -lX11 -lpthread -ldl $(LDFLAGS)
OBJECTS 	:= common.o npclass.o configloader.o pluginfunctions.o debug.o
COMMONHDR	:= ../common/common.h ../npapi-headers/npapi.h ../npapi-headers/npfunctions.h ../npapi-headers/npruntime.h ../npapi-headers/nptypes.h

libpipelight.so: basicplugin.c basicplugin.h configloader.h $(COMMONHDR) $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -z defs -shared basicplugin.c $(OBJECTS) $(LDFLAGS) -o libpipelight.so

common.o: ../common/common.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../common/common.c -o common.o

npclass.o: npclass.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npclass.c -o npclass.o

configloader.o: configloader.c configloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DPREFIX='"$(prefix)"' -c configloader.c -o configloader.o

pluginfunctions.o: pluginfunctions.c basicplugin.h configloader.h debug.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c pluginfunctions.c -o pluginfunctions.o

debug.o: debug.c debug.h basicplugin.h configloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c debug.c -o debug.o

.PHONY: all
all: libpipelight.so

.PHONY: clean
clean:
	rm -f libpipelight.so *.o
