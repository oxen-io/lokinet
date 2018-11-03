# LokiNET bootstrap for Windows

This is a tiny executable that does the same thing as the `lokinet-bootstrap` shell script for Linux, specifically for the purpose of bypassing broken or outdated versions of Schannel that do not support current versions of TLS.

# Building

## requirements

- mbedtls 2.13.0 or later, for both host and windows
- wget for host (to download Netscape CA bundle from cURL website)
- Also included is a patch that can be applied to the mbedtls source to enable features like AES-NI in protected mode, plus some networking fixes for win32

native build:

    $ export INCLUDE=/mingw32/include LIBS=/mingw32/lib # or a different path
    $ export CC=cc # change these if you use clang
    $ export NATIVE_CC=$CC
    $ export WINNT_INCLUDE=$INCLUDE WINNT_LIBS=$LIBS
    $ make prepare;make lokinet-bootstrap

cross-compile build:

    $ export INCLUDE=/usr/local/include LIBS=/usr/local/lib # or a different path
    $ export CC=i686-w64-mingw32-gcc # change these if you use clang, make sure these are in your system $PATH!
    $ export NATIVE_CC=cc
    $ export WINNT_INCLUDE=/path/to/win32/headers WINNT_LIBS=/path/to/win32/libs
    $ make prepare;make lokinet-bootstrap

# Usage

   C:\>lokinet-bootstrap [uri] [local download path]

this is also included in the lokinet installer package.

-despair86
