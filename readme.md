# LLARP

Low Latency Anon Routing Protocol

We'll be ready when we're ready.

[what/why](doc/high-level.txt)
[how](doc/proto_v0.txt)

## Reference Implementation

Build requirements:

* CMake / gmake / ninja
* libsodium >= 1.0.14 
* c++ 17 capable C++ compiler
* c11 capable C compiler


Building:

    $ make

Building in another directory:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    
Building really fast (requires ninja):

    $ mkdir build
    $ cd build 
    $ cmake -GNinja ..
    $ ninja
    
Running:

    $ ./llarpd daemon.ini
