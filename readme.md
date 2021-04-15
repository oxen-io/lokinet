# Lokinet

[Español](readme_es.md) [Русский](readme_ru.md)

Lokinet is the reference implementation of LLARP (low latency anonymous routing protocol), a layer 3 onion routing protocol.

You can learn more about the high level design of LLARP [here](docs/high-level.txt)

And you can read the LLARP protocol specification [here](docs/proto_v0.txt)

You can view documentation on how to get started [here](https://docs.oxen.io/products-built-on-oxen/lokinet) .

[![Build Status](https://ci.oxen.rocks/api/badges/oxen-io/loki-network/status.svg?ref=refs/heads/dev)](https://ci.oxen.rocks/oxen-io/loki-network)

## Building

Build requirements:

* Git
* CMake
* C++ 17 capable C++ compiler
* libuv >= 1.27.0
* libsodium >= 1.0.18
* libcurl (for lokinet-bootstrap)
* libunbound
* libzmq
* sqlite3

### Linux

You do not have to build from source if you are on debian or ubuntu as we have apt repositories with pre-built lokinet packages on `deb.oxen.io`.

You can install these using:

    $ sudo curl -so /etc/apt/trusted.gpg.d/oxen.gpg https://deb.oxen.io/pub.gpg
    $ echo "deb https://deb.oxen.io $(lsb_release -sc) main" | sudo tee /etc/apt/sources.list.d/oxen.list
    $ sudo apt update
    $ sudo apt install lokinet


if you want to build a dev build you can do the following:

    $ sudo apt install build-essential cmake git libcap-dev libcurl4-openssl-dev libuv1-dev libsodium-dev pkg-config
    $ git clone --recursive https://github.com/loki-project/loki-network
    $ cd loki-network
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON
    $ make -j$(nproc)

install:

    $ sudo make install

### macOS

You can get the latest stable macos relase from https://lokinet.org/ or check the releases page on github.

alternatively you can build from source, make sure you have cmake, libuv and xcode command line tools installed:

    $ git clone --recursive https://github.com/loki-project/loki-network
    $ cd loki-network
    $ mkdir build
    $ cd build
    $ cmake .. -DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON
    $ make -j$(sysctl -n hw.ncpu)

install:

    $ sudo make install

### Windows

You can get the latest stable windows release from https://lokinet.org/ or check the releases page on github.

windows builds are cross compiled from debian/ubuntu linux

additional build requirements:

* nsis
* cpack

setup:

    $ sudo apt install build-essential cmake git pkg-config mingw-w64 nsis ninja-build

building:

    $ git clone --recursive https://github.com/loki-project/loki-network
    $ cd loki-network
    $ ./contrib/windows.sh

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

    $ pkg install cmake git pkgconf
    $ git clone --recursive https://github.com/loki-project/loki-network
    $ cd loki-network
    $ mkdir build
    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON -DBUILD_SHARED_DEPS=ON ..
    $ make

install (root):

    # make install

## Usage

### Debian / Ubuntu packages

When running from debian package the following steps are not needed as it is already ready to use.

## Create default config

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

## Running on macOS/UNIX/BSD

**YOU HAVE TO RUN AS ROOT**, run using sudo. Elevated privileges are needed to create the virtual tunnel interface.

The macOS installer places the normal binaries (`lokinet` and `lokinet-bootstrap`) in `/usr/local/bin` which should be in your path, so you can easily use the binaries from your terminal. The installer also nukes your previous config and keys and sets up a fresh config and downloads the latest bootstrap seed.

to run, after you create default config:

    $ sudo lokinet

## Running on Windows

**DO NOT RUN AS ELEVATED USER**, run as normal user.
