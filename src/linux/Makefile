CXX 		:= g++
CXXFLAGS 	:= -Wall -DXP_UNIX=1 -DMOZ_X11=1 -fPIC -std=gnu++0x $(CXXFLAGS)
LDFLAGS 	:= -lX11 -lpthread -ldl $(LDFLAGS)
OBJECTS 	:= common.o basicplugin.o configloader.o diagnostic.o npclass.o nppfunctions.o 
COMMONHDR	:= ../common/common.h ../npapi-headers/npapi.h ../npapi-headers/npfunctions.h ../npapi-headers/npruntime.h ../npapi-headers/nptypes.h

libpipelight.so: $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -z defs -shared $(OBJECTS) $(LDFLAGS) -o libpipelight.so

common.o: ../common/common.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../common/common.c -o common.o

basicplugin.o: basicplugin.c basicplugin.h configloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c basicplugin.c -o basicplugin.o

configloader.o: configloader.c configloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DPREFIX='"$(prefix)"' -c configloader.c -o configloader.o

diagnostic.o: diagnostic.c diagnostic.h basicplugin.h configloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c diagnostic.c -o diagnostic.o

npclass.o: npclass.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npclass.c -o npclass.o

nppfunctions.o: nppfunctions.c basicplugin.h configloader.h diagnostic.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c nppfunctions.c -o nppfunctions.o

.PHONY: all
all: libpipelight.so

.PHONY: clean
clean:
	rm -f libpipelight.so *.o
