CC=i686-w64-mingw32-g++
CFLAGS=-Wall -DXP_WIN=1 -std=c++0x
LDFLAGS=-lversion -lgdi32 -lole32
OBJECTS=communication.o handlemanager.o npclass.o npnfunctions.o

pluginloader.exe: pluginloader.c pluginloader.h $(OBJECTS)
	$(CC) $(CFLAGS) pluginloader.c $(OBJECTS) $(LDFLAGS) -o pluginloader.exe

communication.o: ../communication/communication.c ../communication/communication.h
	$(CC) $(CFLAGS) -c ../communication/communication.c -o communication.o

handlemanager.o: ../handlemanager/handlemanager.c ../handlemanager/handlemanager.h
	$(CC) $(CFLAGS) -c ../handlemanager/handlemanager.c -o handlemanager.o

npclass.o: npclass.c
	$(CC) $(CFLAGS) -c npclass.c -o npclass.o

npnfunctions.o: npnfunctions.c npnfunctions.h
	$(CC) $(CFLAGS) -c npnfunctions.c -o npnfunctions.o

pluginfunctions.o:
	$(CC) $(CFLAGS) -c pluginfunctions.c -o pluginfunctions.o

.PHONY: all
all: pluginloader.exe

.PHONY: clean
clean:
	rm -f pluginloader.exe *.o