
all: debug

SIGN = gpg --sign --detach

TARGETS = llarpd libllarp.so libllarp-static.a
SIGS = $(TARGETS:=.sig)

SHADOW_ROOT ?= $(HOME)/.shadow
SHADOW_BIN=$(SHADOW_ROOT)/bin/shadow
SHADOW_CONFIG=shadow.config.xml
SHADOW_PLUGIN=libshadow-plugin-llarp.so

clean:
	rm -f build.ninja rules.ninja cmake_install.cmake CMakeCache.txt
	rm -rf CMakeFiles
	rm -f $(TARGETS)
	rm -f $(SHADOW_PLUGIN) $(SHADOW_CONFIG)
	rm -f *.sig

debug-configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug 

release-configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DRELEASE_MOTTO="$(shell cat motto.txt)"

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
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DSHADOW=ON

shadow-build: shadow-configure
	ninja clean
	ninja

shadow: shadow-build
	python3 contrib/shadow/genconf.py $(SHADOW_CONFIG)
	$(SHADOW_BIN) -w 16 $(SHADOW_CONFIG)

format:
	clang-format -i $$(find daemon llarp include | grep -E '\.[h,c](pp)?$$')
