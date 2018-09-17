# LokiNet

LokiNet is the reference implementation of LLARP (low latency anonymous routing protocol), a layer 3 onion routing protocol.

You can learn more about the high level design of LLARP [here](doc/high-level.txt)

And you can read the LLARP protocol specification [here](doc/proto_v0.txt)

## Building

To build lokinet see the [lokinet builder](https://github.com/loki-project/lokinet-builder)


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
