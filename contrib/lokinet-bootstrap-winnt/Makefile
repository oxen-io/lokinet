# makefile for windows bootstrap
# requires mbedtls to be installed somewhere, for both native and windows targets
# requires wget to be installed for ca bundle download

# to build: 
# $ [g]make prepare;[g]make lokinet-bootstrap

# set this beforehand if you use clang
CC ?= i686-w64-mingw32-gcc
NATIVE_CC ?= cc

# set these for the native system
INCLUDE ?=
LIBS ?=

# set these for 32-bit windows if cross-compiling
WINNT_INCLUDE ?=
WINNT_LIBS ?=

ifdef LTO
LTO_FLAG = -flto
LTO_LDFLAG = -flto -march=nocona -mfpmath=sse -Ofast
endif

ifndef DEBUG
CFLAGS = -Ofast -march=nocona -mfpmath=sse
LDFLAGS = -s
else
CFLAGS = -O0 -g3
endif

.PHONY: download prepare all default

# windows target only
.c.o:
	$(CC) $(WINNT_INCLUDE) -Iinclude $(LTO_FLAG) $(CFLAGS) $< -c

zpipe: zpipe.c miniz.c
	$(NATIVE_CC) $(INCLUDE) -Iinclude $(LIBS) $^ -s -static -o $@

base64enc: base64enc.c
	$(NATIVE_CC) $(INCLUDE) -Iinclude $(LIBS) $^ -s -static -o $@ -lmbedx509 -lmbedtls -lmbedcrypto

download:
	wget -O ./cacert.pem https://curl.haxx.se/ca/cacert.pem

# I *think* this only work with GNU sed...
prepare: zpipe base64enc download
	./zpipe < cacert.pem > data.enc
	./base64enc < data.enc > out.bin
	sed -ie "s/.\{76\}/&\n/g" out.bin
	sed -i 's/.*/\"&\"/g' out.bin
	sed -i '61,2268d' bootstrap.c
	echo ';' >> out.bin
	sed -i '60r out.bin' bootstrap.c

prepare-testnet: zpipe base64enc download
	./zpipe < cacert.pem > data.enc
	./base64enc < data.enc > out.bin
	sed -ie "s/.\{76\}/&\n/g" out.bin
	sed -i 's/.*/\"&\"/g' out.bin
	sed -i '58,2144d' testnet.c
	echo ';' >> out.bin
	sed -i '57r out.bin' testnet.c

lokinet-bootstrap: bootstrap.o miniz.o http.o header.o chunk.o
	$(CC) $(WINNT_LIBS) $(LDFLAGS) -static $^ -o $@.exe $(LTO_LDFLAG) -lmbedx509 -lmbedtls -lmbedcrypto -lws2_32

clean:
	-@rm lokinet*.exe
	-@rm base64enc
	-@rm zpipe
	-@rm cacert.pem
	-@rm data.enc
	-@rm out.*
	-@rm *.o
