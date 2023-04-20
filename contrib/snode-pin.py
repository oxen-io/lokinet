#!/usr/bin/env python3
#
# a script to generate a lokinet ini file that pins your edges to use only snodes run under the wallet addresses of a whitelist of opers.
#
# usage: ./snode-pin.py walletaddr1 walletaddr2 ... walletaddrN > 00-edges.ini
#
# then copy 00-edges.ini into /var/lib/lokinet/conf.d/
# create that dir if it does not exist.
#

import oxenc
import binascii
import sys
import requests

from collections import defaultdict

addrs = sys.argv[1:]
snodes = defaultdict(set)
jreq = {
    "jsonrpc": "2.0",
    "id": "0",
    "method": "get_service_nodes",
    "params": {"service_node_pubkeys": []},
}

# collect all snodes
resp = requests.post("https://public.loki.foundation/json_rpc", json=jreq)
for snode in resp.json().get("result").get("service_node_states", []):
    addr = snode.get("operator_address", None)
    if addr in addrs:
        snodes[addr].add(snode.get("pubkey_ed25519"))

# print the config snippet to stdout
for oper, addrs in snodes.items():
    print(f"# pin edges to use oper {oper}") 
    print("[network]") 
    print("\n".join(f"strict-connect={oxenc.to_base32z(binascii.unhexlify(addr.strip()))}.snode" for addr in addrs ))
