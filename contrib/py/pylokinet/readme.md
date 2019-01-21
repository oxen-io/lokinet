# pylokinet

lokinet with python 3

    # python3 setup.py install

## bootserv

bootserv is a bootstrap server for accepting and serving RCs

    $ gunicorn -b 0.0.0.0:8000 pylokinet.bootserv:app

## pylokinet instance

obtain `liblokinet-shared.so` from a lokinet build

run (root):
    
    # export LOKINET_ROOT=/tmp/lokinet-instance/
    # export LOKINET_LIB=/path/to/liblokinet-shared.so
    # export LOKINET_BOOTSTRAP_URL=http://bootserv.ip.address.here:8000/bootstrap.signed
    # export LOKINET_PING_URL=http://bootserv.ip.address.here:8000/ping
    # export LOKINET_SUBMIT_URL=http://bootserv.ip.address.here:8000/
    # export LOKINET_IP=public.ip.goes.here
    # export LOKINET_PORT=1090
    # export LOKINET_IFNAME=eth0
    # python3 -m pylokinet.instance