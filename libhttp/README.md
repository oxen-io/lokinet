# liblokiweb (libhttp)

## Building

### requirements

- mbedtls 2.13.0 or later, for both host and target (if cross-compiling)
- wget for host (to download Netscape root certificate store from cURL website)
- Also included is a patch that can be applied to the mbedtls source to enable features like AES-NI in protected mode, plus some networking fixes for win32, see `../contrib/lokinet-bootstrap-winnt/mbedtls-win32.patch`

build:

    $ make prepare; make libhttp.[so|dll]

## Useful build-time variables

- INCLUDE: path to mbedtls headers
- LIBS: path to mbedtls libraries
- SYS_LIBS: system-specific link libraries (`-lsocket -lnsl` on Sun systems, `-lws2_32` [or `-lwsock32` if IPv6 is disabled] on Windows)

## Usage

- include libhttp.h in your source
- link against libhttp.[so|dll]

-rick
