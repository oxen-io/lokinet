REPO := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

EXE = $(REPO)/sarpd
STATIC_LIB = $(REPO)/libsarp.a

STATIC_SRC = $(wildcard $(REPO)/libsarp/*.cpp)
STATIC_OBJ = $(STATIC_SRC:.cpp=.cpp.o)

DAEMON_SRC = $(wildcard $(REPO)/daemon/*.c)
DAEMON_OBJ = $(DAEMON_SRC:.c=.o)

SODIUM_FLAGS = $(shell pkg-config --cflags libsodium)
SODIUM_LIBS = $(shell pkg-config --libs libsodium)

REQUIRED_CFLAGS = $(SODIUM_FLAGS) -I$(REPO)/include -std=c99
REQUIRED_CXXFLAGS = $(SODIUM_FLAGS) -I$(REPO)/include -std=c++17
REQUIRED_LDFLAGS = $(LDFLAGS) -ljemalloc $(SODIUM_LIBS) 
CXXFLAGS := $(REQUIRED_CXXFLAGS)
CFLAGS := $(REQUIRED_CFLAGS)

all: build

build: $(EXE)

$(EXE): $(DAEMON_OBJ) $(STATIC_LIB)
	$(CXX) $(DAEMON_OBJ) $(STATIC_LIB) $(REQUIRED_LDFLAGS) -o $(EXE) 

%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $<.o

$(STATIC_LIB):  $(STATIC_OBJ)
	$(AR) -r $(STATIC_LIB) $(STATIC_OBJ)

clean:
	$(RM) $(DAEMON_OBJ) $(EXE) $(STATIC_OBJ) $(STATIC_LIB)
