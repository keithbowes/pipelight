CFLAGS = -Wall -DXP_WIN=1 -std=c++0x

all : 
        #g++ $(CFLAGS) -shared BasicPlugin.c -o npbasicplugin.so
		i686-w64-mingw32-g++ $(CFLAGS) pluginloader.c  npclass.c npnfunctions.c ../handlemanager/handlemanager.c ../communication/communication.c -o pluginloader.exe -lversion -lgdi32 -lole32

clean :
		rm pluginloader.exe