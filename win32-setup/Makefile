# Makefile for windows install pkg and helper library
# debug/internal builds need not support xp
# release builds *should*, but this does NOT remain a hard
# requirement for cutting releases, it can be pulled at any
# time really

CC=i686-w64-mingw32-gcc
CXX=i686-w64-mingw32-g++
CC64=x86_64-w64-mingw32-gcc
CXX64=x86_64-w64-mingw32-g++
CFLAGS=-Ofast -march=nocona -mfpmath=sse
LIBS=-lws2_32
LDFLAGS=-static

all: regdbhelper.dll lokinet-bootstrap.exe 7z.exe dbghelp tcpv6 tap-win32 qt5-ui
default: all

ifndef RELEASE
regdbhelper.dll:
	$(CC) regdb_helper.c -o $@ -shared -Os -s

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
	cd curl-7.66.0; ./configure --host=i686-w64-mingw32 --target=i686-w64-mingw32 CC=$(CC) CXX=$(CXX) CFLAGS="$(CFLAGS)" LIBS=$(LIBS) --disable-shared --without-zlib --without-ssl --with-mbedtls=$(PWD) --enable-optimize --enable-http --disable-ftp --prefix=$(PWD) --disable-file --disable-ldap  --disable-ldaps  --disable-rtsp --enable-proxy --disable-dict --disable-telnet --disable-tftp --disable-pop3 --disable-imap --disable-smb --disable-smtp --disable-gopher --enable-manual
	$(MAKE) -j4 -C curl-7.66.0 install

lokinet-bootstrap.exe: mbedtls curl dbghelp
	cp bin/curl.exe $@
	wget -O rootcerts.pem https://curl.haxx.se/ca/cacert.pem
	cp ../LICENSE .;unix2dos LICENSE LICENSE

dbghelp:
	wget http://installer.lokinet.org/win32/dbghelp32.dll
	wget http://installer.lokinet.org/win32/dbghelp64.dll

else
regdbhelper.dll:
	wget http://installer.lokinet.org/win32/regdbhelper.dll

lokinet-bootstrap.exe:
	wget http://installer.lokinet.org/win32/lokinet-bootstrap.exe
	wget -O rootcerts.pem https://curl.haxx.se/ca/cacert.pem
	cp ../LICENSE .;unix2dos LICENSE LICENSE

dbghelp:
	wget http://installer.lokinet.org/win32/dbghelp32.dll
	wget http://installer.lokinet.org/win32/dbghelp64.dll
endif

# Common rules
7z.exe:
	wget http://installer.lokinet.org/win32/7z.exe

tcpv6:
	wget http://installer.lokinet.org/win32/inet6.7z

tap-win32:
	wget http://installer.lokinet.org/win32/tap-windows-9.21.2.7z
	mv tap-windows-9.21.2.7z tuntapv9_n6.7z
	wget http://installer.lokinet.org/win32/tap-windows-9.9.2_3.7z
	mv tap-windows-9.9.2_3.7z tuntapv9.7z

libuv:
	git clone https://github.com/despair86/libuv.git
	cd libuv; ./autogen.sh; ./configure --host=i686-w64-mingw32 --target=i686-w64-mingw32 CC=$(CC) CXX=$(CXX) CFLAGS="$(CFLAGS)" --disable-shared --prefix=$(PWD)
	make -C libuv -j4 install
	make -C libuv -j4 distclean
	cd libuv; ./configure --host=x86_64-w64-mingw32 --target=x86_64-w64-mingw32 CC=$(CC64) CXX=$(CXX64) CFLAGS="$(CFLAGS)" --disable-shared
	make -C libuv -j4
	cp libuv/.libs/libuv.a $(PWD)/lib64

qt5-ui:
	wget http://installer.lokinet.org/win32/lokinet-qt5-ui.7z

clean:
	-rm -rf curl-7* include lib mbedtls-2* *.exe *.dll *.pem *.7z
