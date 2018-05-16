# LLARP

Low Latency Anon Routing Protocol

**THIS IS A TOY DO NOT USE UNTIL IT'S NOT A TOY**

This project is "secret" don't tell anyone about it yet. :x

[what/why](doc/high-level.txt)
[how](doc/proto_v0.txt)

## Reference Implementation

Build requirements:

* GNU Make
* pkg-config
* libsodium >= 1.0.14 
* c++ 17 capable C++ compiler
* c99 compliant C compiler

Building:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    
Running:

    $ ./llarpd daemon.ini
