# LokiNet

LokiNet is the reference implementation of LLARP (low latency anonymous routing protocol), a layer 3 onion routing protocol.

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

Client mode:

For simple testing do:

    $ lokinet
   
On systemd based distros you can persist it in the background:

    # systemctl enable --now lokinet-client
   
   
Relay mode:

you can participate as a relay node trivially (for now).

On systemd based linux distros do:

    # systemctl enable --now lokinet-relay

Alternatively:

    # mkdir /usr/local/lokinet
    # cd /usr/local/lokinet
    # lokinet --genconf daemon.ini
    # lokinet --check daemon.ini 
    # lokinet /usr/local/lokinet/daemon.ini
