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
        self._work_over = False
        self._finished = finish

    def done(self):
        self._work_completed = True

    @property
    def working(self):
        return not self._work_completed and not self._work_over

    def run(self):
        time.sleep(self._wait_for)
        self._work_over = True
        if self._finished:
            self._finished(self._work_completed)

def hook(ctx, good):
    ctx.stop()
    assert good

def lokinet_run(wait_for, *, custom_nodedb=False, pin_hops=False, snode="55fxrybf3jtausbnmxpgwcsz9t8qkf5pr8t5f4xyto4omjrkorpy.snode:35520"):
    ctx = Lokinet()
    waiter = Waiter(wait_for, lambda good: hook(ctx, good))
    waiter.start()
    # pin first hops
    if pin_hops:
        for hop in ('55fxngtbfyfsjxy1tnw6qruzjm64rr96pews7bmg5an39zihnrxo.snode', '55fxypc8dkw7t364ekxu1hehg1xuonnyitfdyijosso3agd3g4yo.snode'):
            ctx.set_config_opt('network', 'strict-connect', hop)
    ctx.set_config_opt('router', 'min-routers', '2')
    ctx.set_config_opt('network', 'reachable', 'false')
    ctx.set_config_opt('api', 'enabled', 'false')
    ctx.set_config_opt('logging', 'level', 'info')
    ctx.set_config_opt('network', 'profiling', 'false')

    if custom_nodedb:
        ctx.nodedb_load = load_nodedb
        ctx.nodedb_store = store_nodedb
        ctx.nodedb_del = del_nodedb_entry
    req = requests.get("https://seed.lokinet.org/testnet.signed", stream=True)
    ctx.add_bootstrap_rc(req.content)
    print("starting....")
    ctx.start()
    while not ctx.wait_for_ready(100):
        assert waiter.working
    print(f"we are {ctx.localaddr()}")
    id = None
    try:
        addr, port, id = ctx.resolve(snode, wait_for)
        print(f"resolved {snode} as {addr}:{port} on {id}")
        resp = requests.get(f"https://{addr}:{port}/", verify=False)
        print(resp.text)
    finally:
        if id:
            ctx.unresolve(id)
    waiter.done()
    del ctx

if __name__ == '__main__':
    lokinet_run(30)
