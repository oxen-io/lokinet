
all: debug

SIGN = gpg --sign --detach

TARGETS = llarpd libllarp.so libllarp-static.a
SIGS = $(TARGETS:=.sig)

clean:
	rm -f build.ninja rules.ninja cmake_install.cmake CMakeCache.txt
	rm -rf CMakeFiles
	rm -f $(TARGETS)
	rm -f *.sig

debug-configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DASAN=true

release-configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DRELEASE_MOTTO="$(shell cat motto.txt)"

configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug

build: configure
	ninja

debug: debug-configure
	ninja

release-compile: release-configure
	ninja
	strip $(TARGETS)

$(TARGETS): release-compile

%.sig: $(TARGETS)
	$(SIGN) $*

release: $(SIGS)

format:
	clang-format -i $$(find daemon llarp include | grep -E '\.[h,c](pp)?$$')
