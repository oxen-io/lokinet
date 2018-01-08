REPO := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

EXE = $(REPO)/sarpd
STATIC_LIB = $(REPO)/libsarp.a

STATIC_SRC_CPP = $(wildcard $(REPO)/libsarp/*.cpp)
STATIC_SRC_C = $(wildcard $(REPO)/libsarp/*.c)

STATIC_OBJ = $(STATIC_SRC_CPP:.cpp=.cpp.o) $(STATIC_SRC_C:.c=.c.o)

DAEMON_SRC = $(wildcard $(REPO)/daemon/*.c)
DAEMON_OBJ = $(DAEMON_SRC:.c=.c.o)

SODIUM_FLAGS = $(shell pkg-config --cflags libsodium)
SODIUM_LIBS = $(shell pkg-config --libs libsodium)

LIBUV_FLAGS = $(shell pkg-config --cflags libuv)
LIBUV_LIBS = $(shell pkg-config --libs libuv)

REQUIRED_CFLAGS = $(LIBUV_FLAGS) $(SODIUM_FLAGS) -I$(REPO)/include -std=c99 $(CFLAGS)
REQUIRED_CXXFLAGS = $(LIBUV_FLAGS) $(SODIUM_FLAGS) -I$(REPO)/include -std=c++17 $(CXXFLAGS)
REQUIRED_LDFLAGS = $(LDFLAGS) -ljemalloc $(SODIUM_LIBS) $(LIBUV_LIBS)

all: build

build: $(EXE)

$(EXE): $(DAEMON_OBJ) $(STATIC_LIB)
	$(CXX) $(DAEMON_OBJ) $(STATIC_LIB) $(REQUIRED_LDFLAGS) -o $(EXE) 

%.cpp.o: %.cpp
	$(CXX) $(REQUIRED_CXXFLAGS) -c $< -o $<.o

%.c.o: %.c
	$(CC) $(REQUIRED_CFLAGS) -c $< -o $<.o

$(STATIC_LIB):  $(STATIC_OBJ)
	$(AR) -r $(STATIC_LIB) $(STATIC_OBJ)

clean:
	$(RM) $(DAEMON_OBJ) $(EXE) $(STATIC_OBJ) $(STATIC_LIB)
