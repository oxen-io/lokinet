# Makefile for windows install pkg and helper library

CC=i686-w64-mingw32-gcc
CXX=i686-w64-mingw32-g++
CFLAGS=-Ofast -march=nocona -mfpmath=sse
LIBS=-lws2_32
LDFLAGS=-static

ifndef RELEASE
all: regdbhelper.dll lokinet-bootstrap.exe
default: all

regdbhelper.dll:
	i686-w64-mingw32-gcc regdb_helper.c -o $@ -shared -Os -s

mbedtls:
	wget https://tls.mbed.org/download/mbedtls-2.16.3-apache.tgz
	tar xvf mbedtls-2.16.3-apache.tgz
	patch -p0 -d mbedtls-2.16.3 < mbedtls-win32.patch
	$(MAKE) -j4 -C mbedtls-2.16.3/library CC=$(CC) CXX=$(CXX) CFLAGS="$(CFLAGS)" LDFLAGS=$(LIBS)
	mkdir -p lib; mkdir -p include
	cp mbedtls-2.16.3/library/*.a lib
	cp -r mbedtls-2.16.3/include/mbedtls include

curl:
	wget https://curl.haxx.se/download/curl-7.66.0.tar.xz
	tar xvf curl-7.66.0.tar.xz
	patch -p1 < curl-win32.patch
	cd curl-7.66.0; ./configure --host=i686-w64-mingw32 --target=i686-w64-mingw32 CC=$(CC) CXX=$(CXX) CFLAGS="$(CFLAGS)" LIBS=$(LIBS) --disable-shared --without-zlib --without-ssl --with-mbedtls=$(PWD) --enable-optimize --enable-http --disable-ftp --disable-file --disable-ldap  --disable-ldaps  --disable-rtsp --enable-proxy --disable-dict --disable-telnet --disable-tftp --disable-pop3 --disable-imap --disable-smb --disable-smtp --disable-gopher --enable-manual
	$(MAKE) -j4 -C curl-7.66.0

lokinet-bootstrap.exe: mbedtls curl
	cp curl-7.66.0/src/curl.exe $@
	wget -O rootcerts.pem https://curl.haxx.se/ca/cacert.pem
else

all: regdbhelper.dll lokinet-bootstrap.exe

regdbhelper.dll:
	scp despair@10.10.10.6:loki-network/win32-setup/*.dll .

lokinet-bootstrap.exe:
	scp despair@10.10.10.6:loki-network/win32-setup/*.exe .
	wget -O rootcerts.pem https://curl.haxx.se/ca/cacert.pem
endif

clean:
	-rm -rf curl-7* include lib mbedtls-2* *.exe *.dll *.pem
