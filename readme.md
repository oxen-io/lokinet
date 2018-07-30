# LokiNet

Lokinet is a private, decentralized and Sybil resistant overlay network for the internet, it uses a new routing protocol called LLARP (Low latency anonymous routing protocol)

You can learn more about the high level design of LLARP [here](doc/high-level.txt)

And you can read the LLARP protocol specification [here](doc/proto_v0.txt)

## Building

    # or your OS or distro's package manager
    $ sudo apt install build-essential libtool autoconf cmake git
    $ git clone --recursive https://github.com/loki-project/lokinet-builder
    $ cd lokinet-builder
    $ make 

## Building on Windows (mingw-w64 native, or wow64/linux/unix cross-compiler)

    #i686 or x86_64
    #if cross-compiling from anywhere other than wow64, export CC and CXX to
    #$ARCH-w64-mingw32-g[cc++] respectively
    $ pacman -Sy base-devel mingw-w64-$ARCH-toolchain git libtool autoconf cmake
    $ git clone --recursive https://github.com/loki-project/lokinet-builder
    $ cd lokinet-builder
    $ make ensure sodium
    $ cd build
    $ cmake ../deps/llarp -DSODIUM_LIBRARIES=./prefix/lib/libsodium.a -DSODIUM_INCLUDE_DIR=./prefix/include -G "Unix Makefiles" -DHAVE_CXX17_FILESYSTEM=ON
    $ make
    $ cp llarpd ../lokinet.exe

## Building on Windows using Microsoft C/C++ (Visual Studio 2017)

* clone https://github.com/loki-project/lokinet-builder from git-bash or whatever git browser you use
* open `%CLONE_PATH%/lokinet-builder/deps/sodium/builds/msvc/vs2017/libsodium.sln` and build one of the targets
* create a `build` folder in `%CLONE_PATH%/lokinet-builder`
* run cmake-gui from `%CLONE_PATH%/lokinet-builder/deps/llarp` as the source directory
  * define `SODIUM_LIB`  to `%CLONE_PATH%/lokinet-builder/deps/sodium/bin/win32/%CONFIG%/%TOOLSET%/%TARGET%/libsodium.lib`
  * define `SODIUM_INCLUDE_DIR` to `%CLONE_PATH%/lokinet-builder/deps/sodium/src/libsodium/include`
  * define `HAVE_CXX17_FILESYSTEM` to `TRUE`
  * select `Visual Studio 2017 15 %ARCH%` as the generator
  * enter a custom toolset if desired (usually `v141_xp`)
* generate the developer studio project files and open in the IDE
* select a configuration
* press F7 to build everything

## Running

    $ ./lokinet

or press `Debug`/`Local Windows Debugger` in the visual studio standard toolbar

### Development

Please note development builds are likely to be unstable.

##### Build requirements:

* CMake
* ninja (for unit testing with Google Tests)
* libsodium >= 1.0.14 (included here)
* c++ 11 capable C++ compiler
* if you have C++17 `<filesystem>` or `<experimental/filesystem>` declared and defined in your platform's C++ toolchain, re-run CMake (in `lokinet-builder/build`) with `-DHAVE_CXX17_FILESYSTEM=ON`.
* since each platform seems to have its own idea of where `std::[experimental::]filesystem` is defined, you will need to manually specify its library in `LDFLAGS` or `CMAKE_x_LINKER_FLAGS` as well.

##### Windows:

* Mingw-w64 is recommended for 32 or 64-bit builds.
* It *is* possible to compile with Microsoft C++ (v19 or later from VS2015+).
* `cpp17::filesystem` in `vendor/cppbackport-master` is not available for Windows.
 
#### Boxed warning
<div style="border:5px solid #f00;padding:5px">
<p>Inbound sessions are unsupported on Windows Server systems.</p>
<p><strong><em>Ignore this warning at your own peril.</em></strong></p>
</div>

##### Building a debug build:

    #in lokinet-builder/
    $ cd build
    $ make