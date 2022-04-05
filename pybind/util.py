#!/usr/bin/env python3

import pylokinet
from pylokinet import Context as Lokinet

pylokinet.set_net_id("gamma")

import time

import requests

def load_nodedb():
    return dict()

def store_nodedb(k, v):
    print(f'store nodedb entry: {k} {v}')

def del_nodedb_entry(k):
    print(f'delete entry from nodedb: {k}')

def lokinet_run(wait_for, snode="55fxnzi5myfm1ypnx5w4c5714jgo15tks45dgf6isgut9onnduxo.snode:35515"):
    endat = time.time() + wait_for
    ctx = Lokinet()
    ctx.set_config_opt('network', 'hops', '2')
    ctx.set_config_opt('network', 'reachable', 'false')
    ctx.set_config_opt('api', 'enabled', 'false')
    ctx.set_config_opt('logging', 'level', 'trace')
    ctx.set_config_opt('network', 'profiling', 'false')
     
    ctx.nodedb_load = load_nodedb
    ctx.nodedb_store = store_nodedb
    ctx.nodedb_del = del_nodedb_entry
    req = requests.get("https://seed.lokinet.org/testnet.signed", stream=True)
    ctx.add_bootstrap_rc(req.content)
    print("starting....")
    ctx.start()
    while not ctx.wait_for_ready(100):
        assert endat >= time.time()
    print(f"we are {ctx.localaddr()}")
    id = None
    try:
        addr, port, id = ctx.resolve(snode)
        print(f"resolved {snode} as {addr}:{port} on {id}")
        resp = requests.get(f"https://{addr}:{port}/", verify=False)
        print(resp.text)
    finally:
        if id:
            ctx.unresolve(id)

if __name__ == '__main__':
    lokinet_run(5)
