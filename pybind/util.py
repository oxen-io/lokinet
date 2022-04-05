#!/usr/bin/env python3

import pylokinet
from pylokinet import Context as Lokinet

from threading import Thread

import requests
import time

pylokinet.set_net_id("gamma")

def load_nodedb():
    return dict()

def store_nodedb(k, v):
    print(f'store nodedb entry: {k} {v}')

def del_nodedb_entry(k):
    print(f'delete entry from nodedb: {k}')

class Waiter(Thread):

    def __init__(self, wait_for=1000, finish=None):
        Thread.__init__(self)
        self._wait_for = wait_for
        self._work_completed = False
        self._finished = finish

    def done(self):
        self._work_completed = True

    def run(self):
        time.sleep(self._wait_for)
        if self._finished:
            self._finished(self._work_completed)

def _ended(ctx, good):
    ctx.stop()
    assert good

def lokinet_run(wait_for, snode="55fxnzi5myfm1ypnx5w4c5714jgo15tks45dgf6isgut9onnduxo.snode:35515"):
    ctx = Lokinet()
    waiter = Waiter(wait_for, lambda good: _ended(ctx, good))
    waiter.start()
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
        pass
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
    waiter.done()

if __name__ == '__main__':
    lokinet_run(30)
