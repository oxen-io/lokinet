# Lokinet

[Español](readme_es.md)

Lokinet is the reference implementation of LLARP (low latency anonymous routing protocol), a layer 3 onion routing protocol.

You can learn more about the high level design of LLARP [here](docs/high-level.txt)

And you can read the LLARP protocol specification [here](docs/proto_v0.txt)

You can view documentation on how to get started [here](https://loki-project.github.io/loki-docs/Lokinet/LokinetOverview/) .

[![Build Status](https://drone.lokinet.dev/api/badges/loki-project/loki-network/status.svg?ref=refs/heads/master)](https://drone.lokinet.dev/loki-project/loki-network)

## Usage

See the [documentation](https://loki-project.github.io/loki-docs/Lokinet/LokinetOverview/) on how to get started.

Also read the [Public Testing Guide](https://lokidocs.com/Lokinet/Guides/PublicTestingGuide/#1-lokinet-installation) for installation and other helpful information.

### Create default config

to configure as client:

    $ lokinet -g
    $ lokinet-bootstrap

to configure as relay:

    $ lokinet -r -g
    $ lokinet-bootstrap


## Running on Linux

**DO NOT RUN AS ROOT**, run as normal user. This requires the binary to have the proper setcaps set by `make install` on the binary.

to run, after you create default config:

    $ lokinet

## Running on MacOS/UNIX/BSD

**YOU HAVE TO RUN AS ROOT**, run using sudo. Elevated privileges are needed to create the virtual tunnel interface.

The MacOS installer places the normal binaries (`lokinet` and `lokinet-bootstrap`) in `/usr/local/bin` which should be in your path, so you can easily use the binaries from your terminal. The installer also nukes your previous config and keys and sets up a fresh config and downloads the latest bootstrap seed.

to run, after you create default config:

    $ sudo lokinet

## Running on Windows

**DO NOT RUN AS ELEVATED USER**, run as normal user.

## Building

Build requirements:

* Git
* CMake
* C++ 17 capable C++ compiler
* libuv >= 1.27.0
* libsodium >= 1.0.18
* libunbound
* libzmq
* sqlite3

### Linux

build:

    $ sudo apt install build-essential cmake git libcap-dev curl libuv1-dev libsodium-dev pkg-config
    $ git clone --recursive https://github.com/loki-project/loki-network
    $ cd loki-network
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON
    $ make -j$(nproc)

install:

    $ sudo make install

### MacOS

build:
    make sure you have cmake, libuv and xcode command line tools installed

    $ git clone --recursive https://github.com/loki-project/loki-network
    $ cd loki-network
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON
    $ make -j$(nproc)

install:

    $ sudo make install

### Windows

windows builds are cross compiled from ubuntu linux

additional build requirements:

* nsis
* cpack

setup:

    $ sudo apt install build-essential cmake git pkg-config mingw-w64 nsis

building:

    $ git clone --recursive https://github.com/loki-project/loki-network
    $ cd loki-network
    $ mkdir build-windows
    $ cd build-windows
    $ cmake -DBUILD_STATIC_DEPS=ON -DNATIVE_BUILD=OFF -DCMAKE_BUILD_TYPE=Release -DBUILD_PACKAGE=ON -DCMAKE_TOOLCHAIN_FILE='../contrib/cross/mingw64.cmake' -DWITH_TESTS=OFF -DCMAKE_CROSSCOMPILING=ON ..
    $ cpack -D CPACK_MONOLITHIC_INSTALL=1 -G NSIS ..

### Solaris 2.10+

NOTE: Oracle Solaris users need to download/compile the TAP driver from http://www.whiteboard.ne.jp/~admin2/tuntap/

The generated binaries _may_ work on Solaris 2.10 or earlier, you're on your own. (Recommended: `-static-libstdc++ -static-libgcc`, and the TAP driver if not already installed on the target system.)

Building on a v2.10 or earlier system is unsupported, and may not even work; recent GCC releases have progressively dropped support for older system releases.

build:

    $ sudo pkg install build-essential gcc8 wget tuntap cmake (optional: ninja ccache - from omnios extra) (OmniOS CE)
    $ sudo pkg install base-developer-utilities developer-gnu developer-studio-utilities gcc-7 wget cmake (Oracle Solaris, see note)
    $ sudo pkg install build-essential wget gcc-8 documentation/tuntap header-tun tun (optional: ninja ccache) (all other SunOS)
    $ git clone --recursive https://github.com/loki-project/loki-network
    $ cd loki-network
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make -j$(nproc)

install:

    $ sudo make install

### FreeBSD

build:

    $ pkg install cmake git curl libuv libsodium pkgconf libunbound
    $ git clone --recursive https://github.com/loki-project/loki-network
    $ cd loki-network
    $ mkdir build
    $ cmake -DCMAKE_BUILD_TYPE=Release ..
    $ make

install (root):

    # make install
