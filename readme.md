# LokiNet

LokiNet is the reference implementation of LLARP (low latency anonymous routing protocol), a layer 3 onion routing protocol.

You can learn more about the high level design of LLARP [here](docs/high-level.txt)

And you can read the LLARP protocol specification [here](docs/proto_v0.txt)

![build status](https://gitlab.com/lokiproject/loki-network/badges/master/pipeline.svg "build status")

## Building

Build requirements:

* GNU Make
* CMake
* C++ 17 capable C++ compiler
* rapidjson (if enabling jsonrpc server)

To build:

    $ sudo apt install build-essential cmake git libcap-dev wget rapidjson-dev
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ make -j8 JSONRPC=ON
    $ sudo make install

## Running

**DO NOT RUN AS ROOT**, run as normal user.

to run as client:

    $ lokinet -g
    $ lokinet-bootstrap
    $ lokinet

to run as relay:

   $ lokinet -r -g
   $ lokinet-bootstrap
   $ lokinet

## Usage

see the [documentation](https://loki-project.github.io/loki-docs/Lokinet/LokinetOverview/) on how to get started.
