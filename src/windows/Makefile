CXX 		:= i686-w64-mingw32-g++
CXXFLAGS	:= -Wall -DXP_WIN=1 -std=c++0x $(CXXFLAGS)
LDFLAGS 	:= -lversion -lgdi32 -lole32 #$(LDFLAGS)
OBJECTS 	:= communication.o handlemanager.o npclass.o npnfunctions.o

pluginloader.exe: pluginloader.c pluginloader.h $(OBJECTS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) pluginloader.c $(OBJECTS) $(LDFLAGS) -o pluginloader.exe

communication.o: ../communication/communication.c ../communication/communication.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../communication/communication.c -o communication.o

handlemanager.o: ../handlemanager/handlemanager.c ../handlemanager/handlemanager.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c ../handlemanager/handlemanager.c -o handlemanager.o

npclass.o: npclass.c
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npclass.c -o npclass.o

npnfunctions.o: npnfunctions.c
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c npnfunctions.c -o npnfunctions.o

pluginfunctions.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c pluginfunctions.c -o pluginfunctions.o

.PHONY: all
all: pluginloader.exe

.PHONY: clean
clean:
	rm -f pluginloader.exe *.o