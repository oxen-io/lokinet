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

To build:

    $ sudo apt install build-essential cmake git libcap-dev
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ make -j8

## Running

    $ ./lokinet -g
    $ ./lokinet-bootstrap
    $ ./lokinet

## Usage

see the [documentation](https://loki-project.github.io/loki-docs/Lokinet/LokinetOverview/) on how to get started.
