# LokiNet

Lokinet is a private, decentralized and Sybil resistant overlay network for the internet, it uses a new routing protocol called LLARP (Low latency anonymous routing protocol)

You can learn more about the high level design of LLARP [here](doc/high-level.txt)
And you can read the LLARP protocol specification [here](doc/proto_v0.txt)

## Building

You have 2 ways the build this project

### Recommended Method (for stable builds)

    $ sudo apt install build-essential libtool autoconf cmake git
    $ git clone --recursive https://github.com/majestrate/llarpd-builder
    $ cd llarpd-builder
    $ make 

### Development build method

Please note development builds are likely to be unstable 

Build requirements:

* CMake
* ninja
* libsodium >= 1.0.14 
* c++ 11 capable C++ compiler


Building a debug build:

    $ make


## Running

Right now the reference daemon connects to nodes you tell it to and that's it.

If you built using the recommended way just run:

    $ ./llarpd

It'll attempt to connect to a test node I run and keep the session alive.
That's it.

If you built using the dev build you are expected to configure the daemon yourself.
