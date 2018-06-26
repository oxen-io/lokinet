
all: debug

SIGN = gpg --sign --detach

REPO := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))


TARGETS = llarpd
SIGS = $(TARGETS:=.sig)

SHADOW_ROOT ?= $(HOME)/.shadow
SHADOW_BIN=$(SHADOW_ROOT)/bin/shadow
SHADOW_CONFIG=$(REPO)/shadow.config.xml
SHADOW_PLUGIN=$(REPO)/libshadow-plugin-llarp.so
SHADOW_LOG=$(REPO)/shadow.log.txt

TESTNET_ROOT=$(REPO)/testnet_tmp
TESTNET_CONF=$(TESTNET_ROOT)/supervisor.conf
TESTNET_LOG=$(TESTNET_ROOT)/testnet.log

clean:
	rm -f build.ninja rules.ninja cmake_install.cmake CMakeCache.txt
	rm -rf CMakeFiles
	rm -f $(TARGETS)
	rm -f $(SHADOW_PLUGIN) $(SHADOW_CONFIG)
	rm -f *.sig

debug-configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DWITH_TESTS=ON

release-configure: clean
	cmake -GNinja -DSTATIC_LINK=ON -DCMAKE_BUILD_TYPE=Release -DRELEASE_MOTTO="$(shell cat motto.txt)"

debug: debug-configure
	ninja
	ninja test

release-compile: release-configure
	ninja
	strip $(TARGETS)

$(TARGETS): release-compile

%.sig: $(TARGETS)
	$(SIGN) $*

release: $(SIGS)

shadow-configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DSHADOW=ON

shadow-build: shadow-configure
	ninja clean
	ninja

shadow: shadow-build
	python3 contrib/shadow/genconf.py $(SHADOW_CONFIG)
	bash -c "$(SHADOW_BIN) -w $$(cat /proc/cpuinfo | grep processor | wc -l) $(SHADOW_CONFIG) &> $(SHADOW_LOG) || true"

testnet-configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug

testnet-build: testnet-configure
	ninja

testnet: testnet-build
	mkdir -p $(TESTNET_ROOT)
	python3 contrib/testnet/genconf.py --bin=$(REPO)/llarpd --svc=10 --clients=1 --dir=$(TESTNET_ROOT) --out $(TESTNET_CONF)
	supervisord -n -d $(TESTNET_ROOT) -l $(TESTNET_LOG) -c $(TESTNET_CONF)

test: debug-configure
	ninja
	ninja test

format:
	clang-format -i $$(find daemon llarp include | grep -E '\.[h,c](pp)?$$')
