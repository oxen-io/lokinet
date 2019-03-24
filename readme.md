# LokiNet

LokiNet is the reference implementation of LLARP (low latency anonymous routing protocol), a layer 3 onion routing protocol.

You can learn more about the high level design of LLARP [here](docs/high-level.txt)

And you can read the LLARP protocol specification [here](docs/proto_v0.txt)

![build status](https://gitlab.com/lokiproject/loki-network/badges/master/pipeline.svg "build status")

## Building

Build requirements:

* GNU Make
* CMake
* C++ 17 capable C++ compiler
* gcovr (if generating test coverage with gcc)
* IMPORTANT NOTE: To use the optimiser, make sure the default -DNDEBUG macro is removed before generating (see #400)

### Linux

build:

    $ sudo apt install build-essential cmake git libcap-dev wget
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ make -j8

install:

    $ sudo make install

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

    # pkg_add wget cmake git (optional: ninja ccache)
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ gmake -j8

install (root):

    # gmake install

### FreeBSD

build:

    $ pkg install wget cmake git
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ gmake -j8

install (root):

    # gmake install

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

## Running on Linux/UNIX/BSD

**DO NOT RUN AS ROOT**, run as normal user.

to run as client:

    $ lokinet -g
    $ lokinet-bootstrap
    $ lokinet

to run as relay:

    $ lokinet -r -g
    $ lokinet-bootstrap
    $ lokinet


## Running on Windows

**DO NOT RUN AS ELEVATED USER**, run as normal user.

to run as client, run the `run-lokinet.bat` batch file as your normal user.


## Usage

see the [documentation](https://loki-project.github.io/loki-docs/Lokinet/LokinetOverview/) on how to get started.
