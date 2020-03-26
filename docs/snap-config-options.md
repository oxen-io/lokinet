
# snapp config options

## required

### ifname
network interface name 
### ifaddr
ip range of network interface

## optional

### keyfile
the private key to persist address with.
if not specified the address will be ephemeral.
### reachable
bool value that sets if we publish our snapp to the dht
`true`: we are reachable via dht
`false`: we are not reachable via dht
### hops
number of hops in a path, min is `1`, max is `8`, defaults to `4`
### paths
number of paths to maintain at any given time, defaults to `6`.
### blacklist-snode
adds a `.snode` to path build blacklist
### exit-node
specify a `.snode` or `.loki` address to use as an exit broker
### local-dns
address to bind local dns resoler to, defaults to `127.3.2.1:53`
if port is omitted it uses port `53`
### upstream-dns
address to forward non lokinet related queries to. if not set lokinet dns will reply with srvfail.
### mapaddr
perma map `.loki` address to an ip owned by the snapp
to map `whatever.loki` to `10.0.10.10` it can be specified via:
```
mapaddr=whatever.loki:10.0.10.10
```

## compile time optional features

### on-up
path to shell script to call when our interface is up
### on-down
path to shell script to call when our interface is down
### on-ready
path to shell script to call when snapp is first ready
