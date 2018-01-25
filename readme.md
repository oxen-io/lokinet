# LLARP

Low Latency Anon Routing Protocol

[rfc](doc/proto_v0.txt)

## Reference Implementation

Build requirements:

* GNU Make
* pkg-config
* libsodium >= 1.0.14 
* c++ 17 capable C++ compiler
* c99 compliant C compiler

Building:

    $ make
    
Running:

    $ ./llarpd daemon.ini
