# LokiNet

LokiNet is the reference implementation of LLARP (low latency anonymous routing protocol), a layer 3 onion routing protocol.

You can learn more about the high level design of LLARP [here](docs/high-level.txt)

And you can read the LLARP protocol specification [here](docs/proto_v0.txt)

## Building

    $ sudo apt install build-essential libtool autoconf cmake git
    $ git clone --recursive https://github.com/loki-project/lokinet-builder
    $ cd lokinet-builder
    $ make 

## Running

    $ ./lokinet

### Development

Please note development builds are likely to be unstable

Build requirements:

* GNU Make
* CMake
* libsodium >= 1.0.14
* C++ 17 capable C++ compiler

Building a debug build:

## Building

![build status](https://gitlab.com/lokiproject/loki-network/badges/master/pipeline.svg "build status")

use the [lokinet builder](https://github.com/loki-project/lokinet-builder) repo.

## Development 

for a development environment:

    $ sudo apt install git libcap-dev build-essential ninja-build cmake libsodium-dev
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ make -j $(cat /proc/cpuinfo | grep processors | wc -l) # use all the cores lmao
    
## Usage

see the [lokinet-builder](https://github.com/loki-project/lokinet-builder)
