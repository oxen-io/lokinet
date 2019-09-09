# Makefile for windows install pkg and helper library

all: regdbhelper.dll lokinet-bootstrap.exe

default: all

regdbhelper.dll:
	i686-w64-mingw32-gcc regdb_helper.c -o $@ -shared -Os -s

lokinet-bootstrap.exe:
	wget https://snowlight.net/loki/win32-dist/lokinet-bootstrap.exe

clean:
	rm *.dll *.exe
