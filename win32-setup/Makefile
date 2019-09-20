# Makefile for windows install pkg and helper library

all: regdbhelper.dll lokinet-bootstrap.exe

CC=i686-w64-mingw32-gcc
CXX=i686-w64-mingw32-g++
CFLAGS=-Ofast -march=nocona -mfpmath=sse
LIBS=-lws2_32

default: all

regdbhelper.dll:
	i686-w64-mingw32-gcc regdb_helper.c -o $@ -shared -Os -s

mbedtls:
	wget https://tls.mbed.org/download/mbedtls-2.16.3-apache.tgz
	tar xvf mbedtls-2.16.3-apache.tgz
	patch -p0 -d mbedtls-2.16.3 < mbedtls-win32.patch
	$(MAKE) -C mbedtls-2.16.3 CC=$(CC) CXX=$(CXX) CFLAGS="$(CFLAGS)" LDFLAGS=$(LIBS)

#lokinet-bootstrap.exe:
#	wget https://snowlight.net/loki/win32-dist/lokinet-bootstrap.exe

clean:
	rm *.dll *.exe
