# Lokinet

[Español](readme_es.md)

Lokinet is the reference implementation of LLARP (low latency anonymous routing protocol), a layer 3 onion routing protocol.

You can learn more about the high level design of LLARP [here](docs/high-level.txt)

And you can read the LLARP protocol specification [here](docs/proto_v0.txt)

You can view documentation on how to get started [here](https://loki-project.github.io/loki-docs/Lokinet/LokinetOverview/) .

![build status](https://gitlab.com/lokiproject/loki-network/badges/master/pipeline.svg "build status")
![travis-ci](https://travis-ci.org/loki-project/loki-network.svg?branch=master "ci status")

## Usage

See the [documentation](https://loki-project.github.io/loki-docs/Lokinet/LokinetOverview/) on how to get started.

Also read the [Public Testing Guide](https://lokidocs.com/Lokinet/Guides/PublicTestingGuide/#1-lokinet-installation) for installation and other helpful information.

## Running on Linux

**DO NOT RUN AS ROOT**, run as normal user. This requires the binary to have the proper setcaps set by `make install` on the binary.

to run as client:

    $ lokinet -g
    $ lokinet-bootstrap
    $ lokinet

to run as relay:

    $ lokinet -r -g
    $ lokinet-bootstrap
    $ lokinet

## Running on MacOS/UNIX/BSD

**YOU HAVE TO RUN AS ROOT**, run using sudo. Elevated privileges are needed to create the virtual tunnel interface.

The MacOS installer places the normal binaries (`lokinet` and `lokinet-bootstrap`) in `/usr/local/bin` which should be in your path, so you can easily use the binaries from your terminal. The installer also nukes your previous config and keys and sets up a fresh config and downloads the latest bootstrap seed.

to run as client:

    $ lokinet -g
    $ lokinet-bootstrap
    $ sudo lokinet

to run as relay:

    $ lokinet -r -g
    $ lokinet-bootstrap
    $ sudo lokinet


## Running on Windows

**DO NOT RUN AS ELEVATED USER**, run as normal user.

to run as client, run the `run-lokinet.bat` batch file as your normal user.


## Building

Build requirements:

* GNU Make
* CMake
* C++ 14 capable C++ compiler
* gcovr (if generating test coverage with gcc)
* libuv >= 1.27.0
* libsodium >= 1.0.17
* libcurl

### Linux

build:

    $ sudo apt install build-essential cmake git libcap-dev curl libuv1-dev libsodium-dev libcurl4-openssl-dev pkg-config
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ make 

install:

    $ sudo make install


alternatively make a debian package with:

    $ debuild -uc -us -b

this puts the built packages in `../`

### MacOS

build:
    make sure you have cmake, libuv and xcode command line tools installed
    
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ make -j8

install:

    $ sudo make install

### Windows

build (where `$ARCH` is your platform - `i686` or `x86_64`):

    $ pacman -Sy base-devel mingw-w64-$ARCH-toolchain git libtool autoconf mingw-w64-$ARCH-cmake
    $ git clone https://github.com/loki-project/loki-network.git
    $ cd loki-network
    $ mkdir -p build; cd build
    $ cmake .. -DCMAKE_BUILD_TYPE=[Debug|Release] -DSTATIC_LINK_RUNTIME=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -G 'Unix Makefiles'

install (elevated) to `$PROGRAMFILES/lokinet` or `$ProgramFiles(x86)/lokinet`:

    $ make install

if cross-compiling, install mingw-w64 from your distro's package manager, or [build from source](https://sourceforge.net/p/mingw-w64/wiki2/Cross%20Win32%20and%20Win64%20compiler/), then:

    $ mkdir -p build; cd build
    $ export COMPILER=clang # if using clang for windows
    $ cmake .. -DCMAKE_BUILD_TYPE=[Debug|Release] -DSTATIC_LINK_RUNTIME=ON -DCMAKE_CROSSCOMPILING=ON -DCMAKE_TOOLCHAIN_FILE=../contrib/cross/mingw[32].cmake

this will create a static binary that can be installed anywhere, with no other dependency other than libc (minimum v6.1)

### Solaris 2.10+

NOTE: Oracle Solaris users need to download/compile the TAP driver from http://www.whiteboard.ne.jp/~admin2/tuntap/

The generated binaries _may_ work on Solaris 2.10 or earlier, you're on your own. (Recommended: `-static-libstdc++ -static-libgcc`, and the TAP driver if not already installed on the target system.)

Building on a v2.10 or earlier system is unsupported, and may not even work; recent GCC releases have progressively dropped support for older system releases.

build:

    $ sudo pkg install build-essential gcc8 wget tuntap cmake (optional: ninja ccache - from omnios extra) (OmniOS CE)
    $ sudo pkg install base-developer-utilities developer-gnu developer-studio-utilities gcc-7 wget cmake (Oracle Solaris, see note)
    $ sudo pkg install build-essential wget gcc-8 documentation/tuntap header-tun tun (optional: ninja ccache) (all other SunOS)
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ gmake -j8

install:

    $ sudo make install


### NetBSD (and other platforms where pkgsrc is _the_ native package mgr)

TODO: add pkgsrc instructions

### OpenBSD (uses legacy netbsd pkg manager)

build:

    # pkg_add curl cmake git (optional: ninja ccache)
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ gmake -j8

install (root):

    # gmake install

### FreeBSD

build:

    $ pkg install cmake git curl libuv libsodium pkgconf
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ mkdir build
    $ cmake -DCMAKE_BUILD_TYPE=Release ..
    $ make

install (root):

    # make install
