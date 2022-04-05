#!/usr/bin/env python3

import pylokinet
from pylokinet import Context as Lokinet

from threading import Thread

import requests
import time
from datetime import datetime

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
        self.join()

    @property
    def working(self):
        return not self._work_completed and not self._work_over

    def run(self):
        time.sleep(self._wait_for)
        self._work_over = True
        if self._finished:
            self._finished(self._work_completed)

def hook(ctx, good, trial, check=True):
    ctx.stop()
    print(f'[{trial}] test was {"" if good else "no "}bueno')
    if check:
        assert good

def run_lokinet(wait_for, *, trial=0, nodedb=None, pin_hops=list(), target=("55fxrybf3jtausbnmxpgwcsz9t8qkf5pr8t5f4xyto4omjrkorpy.snode", 35520), check=True):
    ctx = Lokinet()
    waiter = Waiter(wait_for, lambda good: hook(ctx, good, trial, check))
    waiter.start()
    # pin first hops
    if pin_hops:
        for hop in pin_hops:
            ctx.set_config_opt('network', 'strict-connect', hop)
        ctx.set_config_opt('router', 'min-routers', f'len(pin_hops)')
        ctx.set_config_opt('router', 'max-routers', f'len(pin_hops)')
    #ctx.set_config_opt('network', 'reachable', 'false')
    ctx.set_config_opt('api', 'enabled', 'false')
    ctx.set_config_opt('logging', 'level', 'none')
    ctx.set_config_opt('network', 'profiling', 'false')

    if nodedb is not None:
        print(f'[{trial}] set up nodedb with {len(nodedb)} nodes')
        ctx.nodedb_load = lambda : dict(nodedb)
        ctx.nodedb_store = nodedb.__setitem__
        ctx.nodedb_del = nodedb.__delitem__
    req = requests.get("https://seed.lokinet.org/testnet.signed", stream=True)
    ctx.add_bootstrap_rc(req.content)
    print(f"[{trial}] starting....")
    ctx.start()
    while not ctx.wait_for_ready(100):
        assert waiter.working
    print(f"[{trial}] we are {ctx.localaddr()}")
    id = None
    success = False
    try:
        print(f'[{trial}] trying to resolve {target[0]}')
        addr, port, id = ctx.resolve(f'{target[0]}:{target[1]}', 5)
        print(f"[{trial}] resolved {target[0]} as {addr}:{port} on {id}")
        resp = requests.get(f"http://{addr}:{port}/", headers={'host': target[0]})
        assert resp.status_code < 300
        success = True
    except Exception as ex:
        print(f'[{trial}] failed: {ex}')
        success = False
    finally:
        if id:
            ctx.unresolve(id)
    waiter.done()
    del ctx
    if nodedb:
        print(f'[{trial}] we have {len(nodedb)} nodedb entries left over')
        #print(f'[{trial}] {list(nodedb.keys())}')
    return success

if __name__ == '__main__':
    db = dict()
    fails = 0
    errors = 0
    good = 0
    times = 30
    timeout = 15
    pylokinet.set_log_level('none')
    for n in range(times):
        try:
            if run_lokinet(timeout, target=('blocks.loki', 80), nodedb=db, check=False, trial=f'{datetime.now()} | test {1+n}'):
                good += 1
            else:
                fails += 1
        except:
            fails += 1
            errors += 1
        time.sleep(1)
    print(f'started up {times-errors} of {times} ({float(float(times-errors) / times) * 100}%)')
    print(f'succeeded {times-fails} of {times} ({float(float(times-fails) / times) * 100}%)')
