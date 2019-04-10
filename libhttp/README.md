# liblokiweb (libhttp)

## Building

### requirements

- mbedtls 2.13.0 or later, for both host and windows
- wget for host (to download Netscape CA bundle from cURL website)
- Also included is a patch that can be applied to the mbedtls source to enable features like AES-NI in protected mode, plus some networking fixes for win32, see `../contrib/lokinet-bootstrap-winnt/mbedtls-win32.patch`

build:

    $ [g]make prepare; [g]make libhttp.[so|dll]

if you have installed mbedtls in a different path, define INCLUDE and LIBS with the path to the mbedtls headers, library search path, and any extra system libraries required (libsocket/libnsl on Sun, `ws2_32.lib` on Windows)

## Usage

- include libhttp.h in your source
- link against libhttp.[so|dll]

-rick