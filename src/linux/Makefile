CXX 		:= g++
CXXFLAGS 	:= -Wall -fPIC -std=gnu++0x $(CXXFLAGS)
LDFLAGS 	:= -lpthread -ldl $(LDFLAGS)
OBJECTS 	:= common.o basicplugin.o configloader.o diagnostic.o npclass.o nppfunctions.o
COMMONHDR	:= ../common/common.h ../npapi-headers/npapi.h ../npapi-headers/npfunctions.h ../npapi-headers/npruntime.h ../npapi-headers/nptypes.h

UNAME_S 	:= $(shell uname -s)
ifneq ($(UNAME_S),Darwin)
	LDFLAGS += -lX11
endif

.PHONY: all
all: libpipelight.so

libpipelight.so: $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -shared $(OBJECTS) $(LDFLAGS) -o libpipelight.so

common.o: ../common/common.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../common/common.c -o common.o

basicplugin.o: basicplugin.c basicplugin.h configloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c basicplugin.c -o basicplugin.o

configloader.o: configloader.c configloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DPIPELIGHT_CONFIG_PATH='"$(prefix)/share/pipelight/configs"' -c configloader.c -o configloader.o

diagnostic.o: diagnostic.c diagnostic.h basicplugin.h configloader.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c diagnostic.c -o diagnostic.o

npclass.o: npclass.c $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npclass.c -o npclass.o

nppfunctions.o: nppfunctions.c basicplugin.h configloader.h diagnostic.h $(COMMONHDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c nppfunctions.c -o nppfunctions.o

.PHONY: clean
clean:
	rm -f libpipelight.so *.o
