CC=g++
CFLAGS=-Wall -DXP_UNIX=1 -DMOZ_X11=1 -fPIC -std=c++0x
LDFLAGS=-ldl -lX11
OBJECTS=communication.o handlemanager.o npclass.o configloader.o pluginfunctions.o

pipelight.so: basicplugin.c basicplugin.h $(OBJECTS)
	$(CC) $(CFLAGS) -z defs -shared basicplugin.c $(OBJECTS) $(LDFLAGS) -o pipelight.so

communication.o: ../communication/communication.c ../communication/communication.h ../handlemanager/handlemanager.h
	$(CC) $(CFLAGS) -c ../communication/communication.c -o communication.o

handlemanager.o: ../handlemanager/handlemanager.c ../handlemanager/handlemanager.h
	$(CC) $(CFLAGS) -c ../handlemanager/handlemanager.c -o handlemanager.o

npclass.o: npclass.c
	$(CC) $(CFLAGS) -c npclass.c -o npclass.o

configloader.o: configloader.c configloader.h
	$(CC) $(CFLAGS) -c configloader.c -o configloader.o

pluginfunctions.o: pluginfunctions.c
	$(CC) $(CFLAGS) -c pluginfunctions.c -o pluginfunctions.o

.PHONY: all
all: pipelight.so

.PHONY: clean
clean:
	rm -f pipelight.so *.o
