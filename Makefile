
all: test

SIGN = gpg --sign --detach

REPO := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

PREFIX ?= /usr/local

CC ?= cc 
CXX ?= c++

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

TESTNET_EXE=$(REPO)/lokinet-testnet
TESTNET_CLIENTS ?= 50
TESTNET_SERVERS ?= 50
TESTNET_DEBUG ?= 0

JSONRPC = OFF

BUILD_ROOT = $(REPO)/build

CONFIG_CMD = $(shell /bin/echo -n "cd '$(BUILD_ROOT)' && " ; /bin/echo -n "cmake -DUSE_LIBABYSS=$(JSONRPC) '$(REPO)'")

SCAN_BUILD ?= scan-build
ANALYZE_CMD = $(shell /bin/echo -n "cd '$(BUILD_ROOT)' && " ; /bin/echo -n "$(SCAN_BUILD) cmake -DUSE_LIBABYSS=$(JSONRPC) '$(REPO)' && cd '$(BUILD_ROOT)' && $(SCAN_BUILD) $(MAKE)")

TARGETS = $(REPO)/lokinet
SIGS = $(TARGETS:=.sig)
EXE = $(BUILD_ROOT)/lokinet
TEST_EXE = $(BUILD_ROOT)/testAll
ABYSS_EXE = $(BUILD_ROOT)/abyss-main

DNS_PORT ?= 53

# PROCS ?= $(shell cat /proc/cpuinfo | grep processor | wc -l)


LINT_FILES = $(wildcard llarp/*.cpp)

LINT_CHECK = $(LINT_FILES:.cpp=.cpp-check)

clean:
	rm -rf $(BUILD_ROOT)
	rm -f $(SHADOW_PLUGIN) $(SHADOW_CONFIG)
	rm -f *.sig
	rm -f *.a *.so

debug-configure:
	mkdir -p '$(BUILD_ROOT)'
	$(CONFIG_CMD) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DDNS_PORT=$(DNS_PORT)

release-configure: clean
	mkdir -p '$(BUILD_ROOT)'	
	$(CONFIG_CMD) -DSTATIC_LINK=ON -DCMAKE_BUILD_TYPE=Release -DRELEASE_MOTTO="$(shell cat motto.txt)" -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX)

debug: debug-configure
	$(MAKE) -C $(BUILD_ROOT)
	cp $(EXE) lokinet

release-compile: release-configure
	$(MAKE) -C $(BUILD_ROOT)
	cp $(EXE) lokinet
	strip $(TARGETS)

$(TARGETS): release-compile

%.sig: $(TARGETS)
	$(SIGN) $*

release: $(SIGS)

shadow-configure: clean
	$(CONFIG_CMD) -DCMAKE_BUILD_TYPE=Debug -DSHADOW=ON -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX)

shadow-build: shadow-configure
	$(MAKE) -C $(BUILD_ROOT) clean
	$(MAKE) -C $(BUILD_ROOT)

shadow-run: shadow-build
	python3 contrib/shadow/genconf.py $(SHADOW_CONFIG)
	bash -c "$(SHADOW_BIN) -w $$(cat /proc/cpuinfo | grep processor | wc -l) $(SHADOW_CONFIG) | $(SHADOW_PARSE)"

shadow-plot: shadow-run
	$(SHADOW_PLOT)

shadow: shadow-plot

testnet-clean: clean
	rm -rf $(TESTNET_ROOT)

testnet-configure: testnet-clean
	$(CONFIG_CMD) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DTESTNET=1

testnet-build: testnet-configure
	$(MAKE) -C $(BUILD_ROOT)

shared-configure: clean
	$(CONFIG_CMD) -DCMAKE_BUILD_TYPE=Debug -DWITH_TESTS=ON -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DWITH_SHARED=ON

shared: shared-configure
	$(MAKE) -C $(BUILD_ROOT)

testnet: 
	cp $(EXE) $(TESTNET_EXE)
	mkdir -p $(TESTNET_ROOT)
	python3 contrib/testnet/genconf.py --bin=$(TESTNET_EXE) --svc=$(TESTNET_SERVERS) --clients=$(TESTNET_CLIENTS) --dir=$(TESTNET_ROOT) --out $(TESTNET_CONF) --connect=4
	LLARP_DEBUG=$(TESTNET_DEBUG) supervisord -n -d $(TESTNET_ROOT) -l $(TESTNET_LOG) -c $(TESTNET_CONF)

test: debug
	$(TEST_EXE)

abyss: debug
	$(ABYSS_EXE)

format:
	clang-format -i $$(find daemon llarp include | grep -E '\.[h,c](pp)?$$')

analyze: clean
	mkdir -p '$(BUILD_ROOT)'
	$(ANALYZE_CMD)

lint: $(LINT_CHECK)

%.cpp-check: %.cpp
	clang-tidy $^ -- -I$(REPO)/include -I$(REPO)/crypto/libntrup/include -I$(REPO)/llarp

install:
	rm -f $(PREFIX)/bin/lokinet
	cp $(EXE) $(PREFIX)/bin/lokinet
	chmod 755 $(PREFIX)/bin/lokinet
	setcap cap_net_admin=+eip $(PREFIX)/bin/lokinet
	rm -f $(PREFIX)/bin/lokinet-bootstrap
	cp $(REPO)/lokinet-bootstrap $(PREFIX)/bin/lokinet-bootstrap
	chmod 755 $(PREFIX)/bin/lokinet-bootstrap
