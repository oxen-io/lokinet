#!/usr/bin/env python3

from argparse import ArgumentParser as AP
import pylokinet
from pylokinet import Context as Lokinet

import os
from timeit import timeit

ap = AP()
ap.add_argument(
    "--bootstrap",
    type=str,
    default=os.path.join(os.environ["HOME"], ".lokinet", "bootstrap.signed"),
)
ap.add_argument(
    "--log-level",
    type=str,
    default=None)


def load_nodedb():
    return dict()

def store_nodedb(k, v):
    print(f'store nodedb entry: {k} {v}')

def del_nodedb_entry(k):
    print(f'delete entry from nodedb: {k}')

def full_lokinet_run(args):
    pylokinet.set_log_level(f'{args.log_level}'.lower())
    ctx = Lokinet()

    ctx.nodedb_load = load_nodedb
    ctx.nodedb_store = store_nodedb
    ctx.nodedb_del = del_nodedb_entry
    
    print(f"using bootstrap {args.bootstrap}")
    with open(args.bootstrap, "rb") as f:
        ctx.add_bootstrap_rc(f.read())
        
    print("starting....")
    ctx.start()
    while not ctx.wait_for_ready(100):
        pass
    print(f"we are {ctx.localaddr()}")


dlt = timeit(lambda: full_lokinet_run(ap.parse_args()), number=1)
print(f"look {dlt}s to do a full lokinet boot")
