REPO := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

EXE = $(REPO)/llarpd
STATIC_LIB = $(REPO)/libllarp.a
SHARED_LIB = $(REPO)/libllarp.so

STATIC_SRC_CPP = $(wildcard $(REPO)/llarp/*.cpp)
STATIC_SRC_C = $(wildcard $(REPO)/llarp/*.c)

STATIC_OBJ = $(STATIC_SRC_CPP:.cpp=.cpp.o) $(STATIC_SRC_C:.c=.c.o)


DAEMON_SRC = $(wildcard $(REPO)/daemon/*.c)
DAEMON_OBJ = $(DAEMON_SRC:.c=.c.o)

TEST_SRC_C = $(wildcard $(REPO)/test/*.c)
TEST_SRC_CPP = $(wildcard $(REPO)/test/*.cpp)

TEST_OBJ_C = $(TEST_SRC_C:.c=.c.bin)
TEST_OBJ_CPP = $(TEST_SRC_CPP:.cpp=.cpp.bin)

TEST_SRC = $(TEST_SRC_C) $(TEST_SRC_CPP)

TEST_OBJ = $(TEST_OBJ_C) $(TEST_OBJ_CPP)

HDRS = $(wildcard $(REPO)/llarp/*.hpp) $(wildcard $(REPO)/include/*/*.h) 
SRCS = $(DAEMON_SRC) $(STATIC_SRC_CPP) $(STATIC_SRC_C) $(TEST_SRC_C) $(TEST_SRC_CPP)
FORMAT = clang-format

SODIUM_FLAGS = $(shell pkg-config --cflags libsodium)
SODIUM_LIBS = $(shell pkg-config --libs libsodium)

LIBUV_FLAGS = $(shell pkg-config --cflags libuv)
LIBUV_LIBS = $(shell pkg-config --libs libuv)

DEBUG_FLAGS = -g

GIT_VERSION ?= $(shell test -e .git && git rev-parse --short HEAD || true)

ifneq ($(GIT_VERSION),"")
	VER_FLAGS=-DGIT_REV=\"$(GIT_VERSION)\"
endif

REQUIRED_CFLAGS = $(LIBUV_FLAGS) $(SODIUM_FLAGS) -I$(REPO)/include -std=c99 $(CFLAGS) $(DEBUG_FLAGS) $(VER_FLAGS) -Wall -fPIC
REQUIRED_CXXFLAGS = $(LIBUV_FLAGS) $(SODIUM_FLAGS) -I$(REPO)/include -std=c++14 $(CXXFLAGS) $(DEBUG_FLAGS) $(VER_FLAGS) -Wall -fPIC
LIB_LDFLAGS = -ljemalloc $(SODIUM_LIBS) $(LIBUV_LIBS) -lm -lstdc++
REQUIRED_LDFLAGS = -L$(REPO) -lllarp 
TEST_LDFLAGS = $(STATIC_LIB) $(LIB_LDFLAGS)

all: build

format:
	$(FORMAT) -style=Google -i $(HDRS) $(SRCS)

build: shared $(EXE)

test: $(TEST_OBJ_CPP) $(TEST_OBJ_C)

shared: $(SHARED_LIB)

$(TEST_SRC): $(STATIC_LIB)


$(REPO)/test/%.cpp.bin: $(REPO)/test/%.cpp
	$(CXX) $(REQUIRED_CXXFLAGS) $< -o $<.bin $(TEST_LDFLAGS)
	mv $<.bin $<.test
	$<.test
	valgrind --tool=callgrind $<.test

$(REPO)/test/%.c.bin: $(REPO)/test/%.c
	$(CC) $(REQUIRED_CFLAGS) $< -o $<.bin $(TEST_LDFLAGS)
	mv $<.bin $<.test
	$<.test
	valgrind --tool=callgrind $<.test

$(EXE): $(DAEMON_OBJ) $(STATIC_LIB)
	$(CXX) $(DAEMON_OBJ) $(STATIC_LIB) $(LIB_LDFLAGS) -o $(EXE) 

%.cpp.o: %.cpp
	$(CXX) $(REQUIRED_CXXFLAGS) -c $< -o $<.o

%.c.o: %.c
	$(CC) $(REQUIRED_CFLAGS) -c $< -o $<.o

$(STATIC_LIB): $(STATIC_OBJ)
	$(AR) -r $(STATIC_LIB) $(STATIC_OBJ)

$(SHARED_LIB): $(STATIC_OBJ)
	$(CXX) -shared -o $(SHARED_LIB) $(STATIC_OBJ) $(LIB_LDFLAGS)

clean:
	$(RM) $(DAEMON_OBJ) $(EXE) $(STATIC_OBJ) $(STATIC_LIB) $(TEST_OBJ) $(SHARED_LIB)
