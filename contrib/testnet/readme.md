loopback testnet scripts

requirements:

* bash
* python3
* supervisord


setup:

make a testnet compatable lokinet build:

    $ cmake -DWITH_TESTNET=ON -B build-testnet -S .
    $ make -C build-testnet lokinet

usage:

from root of repo run:

    $ ./contrib/testnet/testnet.sh build-testnet/daemon/lokinet 20 200
    
this will spin up 20 service nodes and 200 clients
