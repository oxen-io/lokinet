# LokiNet

Lokinet is a private, decentralized and Sybil resistant overlay network for the internet, it uses a new routing protocol called LLARP (Low latency anonymous routing protocol)

You can learn more about the high level design of LLARP [here](doc/high-level.txt)
<<<<<<< Updated upstream

And you can read the LLARP protocol specification [here](doc/proto_v0.txt)
=======
And you can read the LLARP protocol specification [here](doc/proto_v0.txt)

## Building

    $ sudo apt install build-essential libtool autoconf cmake git
    $ git clone --recursive https://github.com/loki-project/lokinet-builder
    $ cd lokinet-builder
    $ make 

## Running

<<<<<<< Updated upstream
    $ ./lokinet
=======
    $ sudo apt install build-essential libtool autoconf cmake git python3-venv
    $ git clone --recursive https://github.com/majestrate/llarpd-builder
    $ cd llarpd-builder
    $ make
>>>>>>> Stashed changes

### Development

Please note development builds are likely to be unstable

Build requirements:

* CMake
* ninja
* libsodium >= 1.0.14
* c++ 11 capable C++ compiler (gcc 7.x+, llvm 3.8+)


Building a debug build:
>>>>>>> Stashed changes

To build lokinet see the [lokinet-builder](https://github.com/loki-project/lokinet-builder) repository.