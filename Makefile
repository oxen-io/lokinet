
all: debug

clean:
	rm -f build.ninja rules.ninja cmake_install.cmake CMakeCache.txt
	rm -rf CMakeFiles

debug-configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DASAN=true

configure: clean
	cmake -GNinja -DCMAKE_BUILD_TYPE=Release

compile: configure
	ninja

debug: debug-configure
	ninja

format:
	clang-format -i $$(find daemon llarp include | grep -E '\.[h,c](pp)?$$')
