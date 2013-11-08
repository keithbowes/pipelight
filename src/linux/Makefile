CXX 		:= g++
CXXFLAGS 	:= -Wall -DXP_UNIX=1 -DMOZ_X11=1 -fPIC -std=gnu++0x $(CXXFLAGS)
LDFLAGS 	:= -lX11 -lpthread -ldl $(LDFLAGS)
OBJECTS 	:= common.o basicplugin.o configloader.o debug.o npclass.o pluginfunctions.o 
COMMONHDR	:= ../common/common.h ../npapi-headers/npapi.h ../npapi-headers/npfunctions.h ../npapi-headers/npruntime.h ../npapi-headers/nptypes.h

libpipelight.so: $(OBJECTS)
	$(CXX) -z defs -shared $(OBJECTS) $(LDFLAGS) -o libpipelight.so

common.o: ../common/common.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../common/common.c -o common.o

basicplugin.o: basicplugin.c basicplugin.h configloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c basicplugin.c -o basicplugin.o

configloader.o: configloader.c configloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DPREFIX='"$(prefix)"' -c configloader.c -o configloader.o

debug.o: debug.c debug.h basicplugin.h configloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c debug.c -o debug.o

npclass.o: npclass.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npclass.c -o npclass.o

pluginfunctions.o: pluginfunctions.c basicplugin.h configloader.h debug.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c pluginfunctions.c -o pluginfunctions.o

.PHONY: all
all: libpipelight.so

.PHONY: clean
clean:
	rm -f libpipelight.so *.o
