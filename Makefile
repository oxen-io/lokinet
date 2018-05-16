
all: clean compile

clean: build.ninja
	ninja clean

build.ninja:
	cmake -GNinja

compile: build.ninja
	ninja
