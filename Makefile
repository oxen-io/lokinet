
all: test

SIGN = gpg --sign --detach

REPO := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

PREFIX ?= /usr/local

CC ?= cc 
CXX ?= c++

TARGETS = lokinet
SIGS = $(TARGETS:=.sig)


SHADOW_ROOT ?= $(HOME)/.shadow
SHADOW_BIN=$(SHADOW_ROOT)/bin/shadow
SHADOW_CONFIG=$(REPO)/shadow.config.xml
SHADOW_PLUGIN=$(REPO)/libshadow-plugin-llarp.so
SHADOW_LOG=$(REPO)/shadow.log.txt

SHADOW_SRC ?= $(HOME)/local/shadow
SHADOW_PARSE ?= python $(SHADOW_SRC)/src/tools/parse-shadow.py - -m 0 --packet-data
SHADOW_PLOT ?= python $(SHADOW_SRC)/src/tools/plot-shadow.py -d $(REPO) LokiNET -c $(SHADOW_CONFIG) -r 10000 -e '.*'

TESTNET_ROOT=/tmp/lokinet_testnet_tmp
TESTNET_CONF=$(TESTNET_ROOT)/supervisor.conf
TESTNET_LOG=$(TESTNET_ROOT)/testnet.log

EXE = $(REPO)/lokinet
TEST_EXE = $(REPO)/testAll

TESTNET_EXE=$(REPO)/lokinet-testnet
TESTNET_CLIENTS ?= 50
TESTNET_SERVERS ?= 50
TESTNET_DEBUG ?= 0

clean:
	rm -f build.ninja rules.ninja cmake_install.cmake CMakeCache.txt
	rm -rf CMakeFiles
	rm -f $(TARGETS) llarpd llarpc dns rcutil testAll
	rm -f $(SHADOW_PLUGIN) $(SHADOW_CONFIG)
	rm -f *.sig
	rm -f *.a *.so

debug-configure: 
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX)

release-configure: clean
	cmake -GNinja -DSTATIC_LINK=ON -DCMAKE_BUILD_TYPE=Release -DRELEASE_MOTTO="$(shell cat motto.txt)" -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX)

debug: debug-configure
	ninja

release-compile: release-configure
	ninja
	strip $(TARGETS)

$(TARGETS): release-compile


%.sig: $(TARGETS)
	$(SIGN) $*

release: $(SIGS)

shadow-configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DSHADOW=ON -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX)

shadow-build: shadow-configure
	ninja clean
	ninja

shadow-run: shadow-build
	python3 contrib/shadow/genconf.py $(SHADOW_CONFIG)
	bash -c "$(SHADOW_BIN) -w $$(cat /proc/cpuinfo | grep processor | wc -l) $(SHADOW_CONFIG) | $(SHADOW_PARSE)"

shadow-plot: shadow-run
	$(SHADOW_PLOT)

shadow: shadow-plot

testnet-clean: clean
	rm -rf $(TESTNET_ROOT)

testnet-configure: testnet-clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX)

testnet-build: testnet-configure
	ninja

shared-configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DWITH_TESTS=ON -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DWITH_SHARED=ON

shared: shared-configure
	ninja

testnet: 
	cp $(EXE) $(TESTNET_EXE)
	mkdir -p $(TESTNET_ROOT)
	python3 contrib/testnet/genconf.py --bin=$(TESTNET_EXE) --svc=$(TESTNET_SERVERS) --clients=$(TESTNET_CLIENTS) --dir=$(TESTNET_ROOT) --out $(TESTNET_CONF) --connect=3
	LLARP_DEBUG=$(TESTNET_DEBUG) supervisord -n -d $(TESTNET_ROOT) -l $(TESTNET_LOG) -c $(TESTNET_CONF)

test: debug
	$(TEST_EXE)

format:
	clang-format -i $$(find daemon llarp include | grep -E '\.[h,c](pp)?$$')

install:
	rm -f $(PREFIX)/bin/lokinet
	cp $(EXE) $(PREFIX)/bin/lokinet
	chmod 755 $(PREFIX)/bin/lokinet
	setcap cap_net_admin=+eip $(PREFIX)/bin/lokinet
