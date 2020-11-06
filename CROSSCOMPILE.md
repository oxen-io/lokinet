# Cross Compile

Currently supported targets:

Tier 1:

These builds are fully automated using [Drone CI](https://drone.io). Guaranteed to be fully reproducible.
* Linux (arm/x86)
* Windows 8+ (32 and 64 bit x86)


Tier 2:

These targets have no build automation available, but do not require patches to build or run.
* Mac OSX (> 10.10)
* Android (arm/x86)
* Apple IOS
* Linux PPC64 (little endian)
* FreeBSD (amd64)

Tier 3:

These targets are somewhat obscure or possibly obsolete, and may require some patching to fix target specific issues.
* Big Endian Linux
* NetBSD
* OpenBSD
* Windows pre-8 (while this is technically possible, the requirement for [cryptographically reproducible builds](https://reproducible-builds.org/) precludes it.)
* UNIX v5 (x86 AMD64)

Unsupported 
(feel free to support this yourself)

we are completely unable to test these targets at all, proceed at your own risk
* AIX
* zOS

## For Windows

To cross compile for windows on non windows platforms run:

    $ make windows

## For Other Linux

## deps

this setup assumes ubuntu

first you need to cross compile and install libuv:

    $ git clone https://github.com/libuv/libuv
    $ mkdir -p build && cd build
    $ export TOOLCHAIN=arm-linux-gnueabihf # or whatever your compiler is
    $ cmake -DCMAKE_C_COMPILER=$(TOOLCHAIN)-gcc-8 -DCMAKE_INSTALL_PREFIX=/usr/$(TOOLCHAIN)
    $ make
    $ sudo make install

## build

To cross compile on linux for another archietecture:

   # for rpi 3
   $ make CROSS=ON TOOLCHAIN=contrib/armhf.toolchain.cmake

   # for ppc64le
   $ make CROSS=ON TOOLCHAIN=contrib/ppc64le.toolchain.cmake
