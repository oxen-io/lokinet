
all: remove-build clean compile

remove-build:
	rm -f build.ninja rules.ninja cmake_install.cmake CMakeCache.txt
	rm -rf CMakeFiles

clean: build.ninja
	ninja clean

build.ninja:
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug

compile: build.ninja
	ninja

format:
	clang-format -i $$(find daemon llarp include | grep -E '\.[h,c](pp)?$$')
