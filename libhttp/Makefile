# makefile for libhttp
# requires mbedtls to be installed somewhere, for both host and target systems
# requires wget to be installed for ca bundle download

# to build: 
# $ [g]make prepare;[g]make libhttp

# set this beforehand if you use clang
# make sure to preset CFLAGS if you use non-ix86 platform
# or non-GNU-compat C compilation system
CC ?= cc
CFLAGS ?= -Ofast -march=nocona -mfpmath=sse

# path to mbedtls headers/libs and system libs
# if you have local copies of libs in this folder,
# try LIBS=-L. (other stuff here)
#
# -lsocket -lnsl on Sun
# -lws2_32 on windows nt
INCLUDE ?=
LIBS ?=

.PHONY: download prepare all default

.c.o:
	$(CC) $(INCLUDE) -fPIC -Iinclude $(CFLAGS) $< -c

zpipe: zpipe.c miniz.c
	$(CC) $(INCLUDE) -Iinclude $(CFLAGS) $^ -s -static -o $@

base64enc: base64enc.c
	$(CC) $(INCLUDE) -Iinclude $(CFLAGS) $^ -s -static -o $@ -lmbedx509 -lmbedtls -lmbedcrypto $(LIBS)

download:
	wget -O ./cacert.pem https://curl.haxx.se/ca/cacert.pem

# I *think* this only work with GNU sed...
prepare: zpipe base64enc download
	./zpipe < cacert.pem > data.enc
	./base64enc < data.enc > out.bin
	sed -ie "s/.\{76\}/&\n/g" out.bin
	sed -i 's/.*/\"&\"/g' out.bin
	sed -i '38,2228d' cacerts.c
	echo ';' >> out.bin
	sed -i '37r out.bin' cacerts.c

libhttp.dll: cacerts.o miniz.o libhttp.o uri.o internal.o
	$(CC) -fPIC $(CFLAGS) $^ -s -shared -o $@ -static -lmbedx509 -lmbedtls -lmbedcrypto $(LIBS)

libhttp.so: cacerts.o miniz.o libhttp.o uri.o internal.o
	$(CC) -fPIC $(CFLAGS) $^ -s -shared -o $@ -static -lmbedx509 -lmbedtls -lmbedcrypto $(LIBS)

clean:
	-@rm base64enc
	-@rm zpipe
	-@rm cacert.pem
	-@rm data.enc
	-@rm out.*
	-@rm *.o
	-@rm *.so
	-@rm *.dll
