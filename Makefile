REPO := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

EXE = $(REPO)/sarpd
LIB = sarp
STATIC_LIB = $(REPO)/lib$(LIB).a

STATIC_SRC = $(wildcard $(REPO)/libsarp/*.c)
STATIC_OBJ = $(STATIC_SRC:.c=.o)

DAEMON_SRC = $(wildcard $(REPO)/daemon/*.c)
DAEMON_OBJ = $(DAEMON_SRC:.c=.o)

PKG = pkg-config

PKGS = libsodium

REQUIRED_CFLAGS = $(CFLAGS) $(shell $(PKG) --cflags $(PKGS)) -I $(REPO)/include -std=c99
REQUIRED_LDFLAGS = $(LDFLAGS) $(shell $(PKG) --libs $(PKGS))

all: build

build: $(EXE)

$(EXE): $(DAEMON_OBJ) $(STATIC_LIB)
	$(CC) $(REQUIRED_CFLAGS) $(REQUIRED_LDFLAGS) -o $(EXE) $(DAEMON_OBJ) $(STATIC_LIB)

$(STATIC_LIB):  $(STATIC_OBJ)
	$(AR) -r $(STATIC_LIB) $(STATIC_OBJ)

%.o: %.c
	$(CC) $(REQUIRED_CFLAGS) -c $^ -o $@

clean:
	$(RM) $(DAEMON_OBJ) $(EXE) $(STATIC_OBJ) $(STATIC_LIB)
