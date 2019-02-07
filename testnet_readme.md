
# loopback testnet


requirements:

    * python 2.7
    * supervisord

Build the testnet binary in testnet mode `(required only if running on loopback network)`

    $ make testnet-build

generate configs and run the testnet with default parameters:

    $ make testnet
    
generate configs with different parameters and run testnet:

    $ make testnet TESTNET_BASEPORT=1900 TESTNET_SERVERS=100 TESTNET_CLIENTS=200 TESNET_IFNAME=eth0
    

environmental variables:

* `TESTNET_SERVERS` number of service nodes to spin up `(default 20)`

* `TESTNET_CLIENTS` number of clients to spin up `(default 200)`

* `TESTNET_IFNAME` interface to bind all nodes to `(default lo)`

* `TESTNET_BASEPORT` starting udp port number to start at for servers `(default 1900)`

* `TESTNET_DEBUG` set to 1 to enable debug logging `(default 0)`

* `TESTNET_IP` explicit ip to use `(default 127.0.0.1)`

* `TESTNET_NETID` explicit netid to use `(default 'loopback')`
