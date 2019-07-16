# Cross Compile

Currently supported targets:

Tier 1:

* Linux (arm/x86)
* Windows (32 and 64 bit x86)
* FreeBSD (amd64)

Tier 2:

* Mac OSX (> 10.10)
* Android (arm/x86)
* Apple IOS
* Linux PPC64 (little endian)

Tier 3:

* Big Endian Linux
* NetBSD
* OpenBSD

Unsupported (feel free to support this yourself)

* AIX
* zOS

## For Windows

To cross compile for windows on non windows platforms run:

    $ make windows

## For Other Linux

To cross compile on linux for another archietecture:

   # for rpi
   $ make CROSS=ON TOOLCHAIN=contrib/armhf.toolchain.cmake

   # for ppc64le
   $ make CROSS=ON TOOLCHAIN=contrib/ppc64le.toolchain.cmake
