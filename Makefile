
all: remove-build clean compile

remove-build:
	rm -f build.ninja

clean: build.ninja
	ninja clean

build.ninja:
	cmake -GNinja -DCMAKE_BUILD_TYPE=Debug

compile: build.ninja
	ninja
