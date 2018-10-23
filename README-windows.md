# lokinet builder for windows

## Building for Windows (mingw-w64 native, or wow64/linux/unix cross-compiler)

    #i686 or x86_64

    $ pacman -Sy base-devel mingw-w64-$ARCH-toolchain git libtool autoconf cmake (or your distro/OS package mgr)
    $ git clone https://github.com/loki-project/loki-network.git
    $ cd loki-network
    $ mkdir -p build; cd build
    $ cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DDNS_PORT=53

    # if cross-compiling do
    $ mkdir -p build; cd build
    $ export COMPILER=clang # if using clang for windows
    $ cmake .. -DCMAKE_BUILD_TYPE=[Debug|Release] -DCMAKE_C_COMPILER=$ARCH-w64-mingw32-[gcc|clang] -DCMAKE_CXX_COMPILER=$ARCH-w64-mingw32-[g|clang]++ -DDNS_PORT=53 -DCMAKE_TOOLCHAIN_FILE=../contrib/cross/mingw[32].cmake

## running

if the machine you run lokinet on has a public address (at the moment) it `will` automatically become a relay, 
otherwise it will run in client mode.


**NEVER** run lokinet with elevated privileges.

to set up a lokinet to start on boot:

    C:\> (not ready yet. TODO: write up some SCM install code in the win32 setup)

alternatively:

set up the configs and bootstrap (first time only):

    C:\> lokinet -g && lokinet-bootstrap
    
run it (foreground):
    
    C:\> lokinet

to force client mode edit `$APPDATA/.lokinet/daemon.ini`

comment out the `[bind]` section, so it looks like this:

    ...
    
    # [bind]
    # {B7F2ECAC-BB10-4736-8BBD-6E9444E27030}=1090


-despair