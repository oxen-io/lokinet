# Lokinet

[Español](readme_es.md) [Русский](readme_ru.md)

Lokinet is the reference implementation of LLARP (low latency anonymous routing protocol), a layer 3 onion routing protocol.

You can learn more about the high level design of LLARP [here](docs/high-level.txt)

And you can read the LLARP protocol specification [here](docs/proto_v0.txt)

You can view documentation on how to get started [here](https://docs.oxen.io/products-built-on-oxen/lokinet) .

A simple demo application that is lokinet "aware" can be found [here](https://github.com/majestrate/lokinet-aware-demos)

[![Build Status](https://ci.oxen.rocks/api/badges/oxen-io/lokinet/status.svg?ref=refs/heads/dev)](https://ci.oxen.rocks/oxen-io/lokinet)

## Building

Build requirements:

* Git
* CMake
* C++ 17 capable C++ compiler
* libuv >= 1.27.0
* libsodium >= 1.0.18
* libssl (for lokinet-bootstrap)
* libcurl (for lokinet-bootstrap)
* libunbound
* libzmq
* cppzmq
* sqlite3

### Linux

You do not have to build from source if you are on debian or ubuntu as we have apt repositories with pre-built lokinet packages on `deb.oxen.io`.

You can install these using:

    $ sudo curl -so /etc/apt/trusted.gpg.d/oxen.gpg https://deb.oxen.io/pub.gpg
    $ echo "deb https://deb.oxen.io $(lsb_release -sc) main" | sudo tee /etc/apt/sources.list.d/oxen.list
    $ sudo apt update
    $ sudo apt install lokinet


If you want to build from source:

    $ sudo apt install build-essential cmake git libcap-dev pkg-config automake libtool libuv1-dev libsodium-dev libzmq3-dev libcurl4-openssl-dev libevent-dev nettle-dev libunbound-dev libsqlite3-dev
    $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ mkdir build
    $ cd build
    $ cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
    $ make -j$(nproc)
    $ sudo make install

### macOS

Lokinet ~~is~~ will be available on the Apple App store. 

Source code compilation of Lokinet by end users is not supported or permitted by apple on their platforms, see [this](contrib/macos/README.txt) for more information. If you find this disagreeable consider using a platform that permits compiling from source.

### Windows

You can get the latest stable windows release from https://lokinet.org/ or check the releases page on github.

windows builds are cross compiled from debian/ubuntu linux

additional build requirements:

* nsis
* cpack

setup:

    $ sudo apt install build-essential cmake git pkg-config mingw-w64 nsis cpack automake libtool

building:

    $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ ./contrib/windows.sh

### FreeBSD

build:

    $ pkg install cmake git pkgconf
    $ git clone --recursive https://github.com/oxen-io/lokinet
    $ cd lokinet
    $ mkdir build
    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON -DBUILD_STATIC_DEPS=ON ..
    $ make

install (root):

    # make install

## Usage

### Debian / Ubuntu packages

When running from debian package the following steps are not needed as it is already ready to use.

## Running on Linux (without debs)

**DO NOT RUN AS ROOT**, run as normal user. 

set up the initial configs:

    $ lokinet -g 
    $ lokinet-bootstrap

after you create default config, run it:

    $ lokinet

This requires the binary to have the proper capabilities which is usually set by `make install` on the binary. If you have errors regarding permissions to open a new interface this can be resolved using:

    $ sudo setcap cap_net_admin,cap_net_bind_service=+eip /usr/local/bin/lokinet

