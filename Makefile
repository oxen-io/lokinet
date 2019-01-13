all: test

SIGN = gpg --sign --detach

REPO := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

prefix = $(DESTDIR)/usr/local

CC ?= cc
CXX ?= c++

SETCAP ?= which setcap && setcap cap_net_admin,cap_net_bind_service=+eip

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

ANDROID_NDK ?= $(HOME)/Android/Ndk
ANDROID_SDK ?= $(HOME)/Android/Sdk
ANDROID_ABI ?= armeabi-v7a
ANDROID_API_LEVEL ?= 18

ANDROID_DIR=$(REPO)/android
JNI_DIR=$(ANDROID_DIR)/jni
ANDROID_MK=$(JNI_DIR)/Android.mk
ANDROID_PROPS=$(ANDROID_DIR)/gradle.properties
ANDROID_LOCAL_PROPS=$(ANDROID_DIR)/local.properties

GRADLE ?= gradle
JAVA_HOME ?= /usr/lib/jvm/default-java

# jsonrpc server
JSONRPC ?= OFF
# native avx2 code
AVX2 ?= OFF
# non x86 target
NON_PC_TARGET ?= OFF
# statically link
STATIC_LINK ?= OFF
# enable network namespace isolation
NETNS ?= OFF
# using clang
CLANG ?= OFF
# cross compile?
CROSS ?= OFF
# build liblokinet-shared.so
SHARED_LIB ?= ON
# enable generating coverage
COVERAGE ?= OFF
COVERAGE_OUTDIR ?= "$(TMPDIR)/lokinet-coverage"
# cmake generator type
CMAKE_GEN ?= Unix Makefiles

BUILD_ROOT = $(REPO)/build

SCAN_BUILD ?= scan-build

CONFIG_CMD = $(shell /bin/echo -n "cd '$(BUILD_ROOT)' && " ; /bin/echo -n "cmake -G'$(CMAKE_GEN)' -DCMAKE_CROSSCOMPILING=$(CROSS) -DUSING_CLANG=$(CLANG) -DSTATIC_LINK=$(STATIC_LINK) -DUSE_NETNS=$(NETNS) -DUSE_AVX2=$(AVX2) -DUSE_LIBABYSS=$(JSONRPC) -DNON_PC_TARGET=$(NON_PC_TARGET) -DWITH_SHARED=$(SHARED_LIB) '$(REPO)'")

ANALYZE_CONFIG_CMD = $(shell /bin/echo -n "cd '$(BUILD_ROOT)' && " ; /bin/echo -n "$(SCAN_BUILD) cmake -G'$(CMAKE_GEN)' -DCMAKE_CROSSCOMPILING=$(CROSS) -DUSING_CLANG=$(CLANG) -DSTATIC_LINK=$(STATIC_LINK) -DUSE_NETNS=$(NETNS) -DUSE_AVX2=$(AVX2) -DUSE_LIBABYSS=$(JSONRPC) -DNON_PC_TARGET=$(NON_PC_TARGET) -DWITH_SHARED=$(SHARED_LIB) '$(REPO)'")

COVERAGE_CONFIG_CMD = $(shell /bin/echo -n "cd '$(BUILD_ROOT)' && " ; /bin/echo -n "cmake -G'$(CMAKE_GEN)' -DCMAKE_CROSSCOMPILING=$(CROSS) -DUSING_CLANG=$(CLANG) -DSTATIC_LINK=$(STATIC_LINK) -DUSE_NETNS=$(NETNS) -DUSE_AVX2=$(AVX2) -DUSE_LIBABYSS=$(JSONRPC) -DNON_PC_TARGET=$(NON_PC_TARGET) -DWITH_SHARED=$(SHARED_LIB) -DWITH_COVERAGE=yes '$(REPO)'")

TARGETS = $(REPO)/lokinet
SIGS = $(TARGETS:=.sig)
EXE = $(BUILD_ROOT)/lokinet
TEST_EXE = $(BUILD_ROOT)/testAll
ABYSS_EXE = $(BUILD_ROOT)/abyss-main

# PROCS ?= $(shell cat /proc/cpuinfo | grep processor | wc -l)


LINT_FILES = $(wildcard llarp/*.cpp)

LINT_CHECK = $(LINT_FILES:.cpp=.cpp-check)

clean:
	rm -f $(TARGETS)
	rm -rf $(BUILD_ROOT)
	rm -f $(SHADOW_PLUGIN) $(SHADOW_CONFIG)
	rm -f *.sig
	rm -f *.a *.so

debug-configure:
	mkdir -p '$(BUILD_ROOT)'
	$(CONFIG_CMD) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_ASM_FLAGS='$(ASFLAGS)' -DCMAKE_C_FLAGS='$(CFLAGS)' -DCMAKE_CXX_FLAGS='$(CXXFLAGS)'

release-configure: clean
	mkdir -p '$(BUILD_ROOT)'
	$(CONFIG_CMD) -DSTATIC_LINK=ON -DCMAKE_BUILD_TYPE=Release -DRELEASE_MOTTO="$(shell cat motto.txt)" -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DCMAKE_ASM_FLAGS='$(ASFLAGS)' -DCMAKE_C_FLAGS='$(CFLAGS)' -DCMAKE_CXX_FLAGS='$(CXXFLAGS)'

debug: debug-configure
	$(MAKE) -C $(BUILD_ROOT)
	cp $(EXE) lokinet
	cp $(BUILD_ROOT)/liblokinet-shared.so liblokinet-shared.so


release-compile: release-configure
	$(MAKE) -C $(BUILD_ROOT)
	cp $(EXE) lokinet
	strip $(TARGETS)

$(TARGETS): release-compile

%.sig: $(TARGETS)
	$(SIGN) $*

release: $(SIGS)

shadow-configure: clean
	mkdir -p $(BUILD_ROOT)
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
	mkdir -p $(BUILD_ROOT)
	$(CONFIG_CMD) -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) -DTESTNET=1

testnet-build: testnet-configure
	$(MAKE) -C $(BUILD_ROOT)

testnet:
	cp $(EXE) $(TESTNET_EXE)
	mkdir -p $(TESTNET_ROOT)
	python3 contrib/testnet/genconf.py --bin=$(TESTNET_EXE) --svc=$(TESTNET_SERVERS) --clients=$(TESTNET_CLIENTS) --dir=$(TESTNET_ROOT) --out $(TESTNET_CONF) --connect=4
	LLARP_DEBUG=$(TESTNET_DEBUG) supervisord -n -d $(TESTNET_ROOT) -l $(TESTNET_LOG) -c $(TESTNET_CONF)

$(TEST_EXE): debug

test: $(TEST_EXE)
	$(TEST_EXE)

android-gradle-prepare:
	rm -f $(ANDROID_PROPS)
	rm -f $(ANDROID_LOCAL_PROPS)
	echo "#auto generated don't modify kthnx" >> $(ANDROID_PROPS)
	echo "lokinetCMake=$(REPO)/CMakeLists.txt" >> $(ANDROID_PROPS)
	echo "org.gradle.parallel=true" >> $(ANDROID_PROPS)
	echo "#auto generated don't modify kthnx" >> $(ANDROID_LOCAL_PROPS)
	echo "sdk.dir=$(ANDROID_SDK)" >> $(ANDROID_LOCAL_PROPS)
	echo "ndk.dir=$(ANDROID_NDK)" >> $(ANDROID_LOCAL_PROPS)

android-gradle: android-gradle-prepare
	cd $(ANDROID_DIR) && JAVA_HOME=$(JAVA_HOME) $(GRADLE) clean assemble

android: android-gradle
	cp -f $(ANDROID_DIR)/build/outputs/apk/*.apk $(REPO)

windows-configure: clean
	mkdir -p '$(BUILD_ROOT)'
	$(CONFIG_CMD) -DCMAKE_CROSSCOMPILING=ON -DCMAKE_TOOLCHAIN_FILE='$(REPO)/contrib/cross/mingw.cmake'  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc-win32 -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++-win32 -DCMAKE_ASM_FLAGS='$(ASFLAGS)' -DCMAKE_C_FLAGS='$(CFLAGS)' -DCMAKE_CXX_FLAGS='$(CXXFLAGS)'

windows: windows-configure
	$(MAKE) -C '$(BUILD_ROOT)'
	cp '$(BUILD_ROOT)/lokinet.exe' '$(REPO)/lokinet.exe'

abyss: debug
	$(ABYSS_EXE)

format:
	clang-format -i $$(find daemon llarp include libabyss | grep -E '\.[h,c](pp)?$$')

analyze-config: clean
	mkdir -p '$(BUILD_ROOT)'
	$(ANALYZE_CONFIG_CMD)

analyze: analyze-config
	cd '$(BUILD_ROOT)' && $(SCAN_BUILD) $(MAKE)

coverage-config: clean
	mkdir -p '$(BUILD_ROOT)'
	$(COVERAGE_CONFIG_CMD)

coverage: coverage-config
	$(MAKE) -C $(BUILD_ROOT) -j 12
	$(TEST_EXE) || true # continue even if tests fail
	mkdir -p "$(COVERAGE_OUTDIR)"
ifeq ($(CLANG),OFF)
	gcovr -r . --branches --html --html-details -o "$(COVERAGE_OUTDIR)/lokinet.html"
else
	llvm-profdata merge default.profraw -output $(BUILD_ROOT)/profdata
	llvm-cov show -format=html -output-dir="$(COVERAGE_OUTDIR)" -instr-profile "$(BUILD_ROOT)/profdata" "$(BUILD_ROOT)/testAll" $(shell find ./llarp -type f)
endif

lint: $(LINT_CHECK)

%.cpp-check: %.cpp
	clang-tidy $^ -- -I$(REPO)/include -I$(REPO)/crypto/include -I$(REPO)/llarp -I$(REPO)/vendor/cppbackport-master/lib

docker-debian:
	docker build -f docker/debian.Dockerfile .

docker-fedora:
	docker build -f docker/fedora.Dockerfile .

debian-configure:
	mkdir -p '$(BUILD_ROOT)'
	$(CONFIG_CMD) -DDEBIAN=ON -DRELEASE_MOTTO="$(shell cat $(REPO)/motto.txt)" -DCMAKE_BUILD_TYPE=Release

debian: debian-configure
	$(MAKE) -C '$(BUILD_ROOT)'
	cp $(EXE) lokinet
	cp $(BUILD_ROOT)/rcutil lokinet-rcutil

debian-test:
	$(TEST_EXE)

install:
	$(MAKE) -C '$(BUILD_ROOT)' install

fuzz-configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Fuzz -DCMAKE_C_COMPILER=afl-gcc -DCMAKE_CXX_COMPILER=afl-g++

fuzz-build: fuzz-configure
	ninja

fuzz: fuzz-build
	$(EXE)

.PHONY: debian-install
