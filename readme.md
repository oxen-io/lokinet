# LokiNet

LokiNet is the reference implementation of LLARP (low latency anonymous routing protocol), a layer 3 onion routing protocol.

You can learn more about the high level design of LLARP [here](doc/high-level.txt)

And you can read the LLARP protocol specification [here](doc/proto_v0.txt)


## Building

![build status](https://gitlab.com/lokiproject/loki-network/badges/master/pipeline.svg "build status")


If you don't have libsodium 1.0.16 or higher use the [lokinet builder](https://github.com/loki-project/lokinet-builder) repo.

Otherwise:

    $ sudo apt install git libcap-dev build-essential ninja-build cmake libsodium-dev
    $ git clone https://github.com/loki-project/loki-network
    $ cd loki-network
    $ make

## Usage

### Windows

Windows only supports client mode so you run `lokinet.exe` and that's it.

### Linux

see the [lokinet-builder](https://github.com/loki-project/lokinet-builder)
