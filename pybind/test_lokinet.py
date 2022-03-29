#!/usr/bin/env python3

import pylokinet
from pylokinet import Context as Lokinet

import os
from timeit import timeit

import requests

def load_nodedb():
    return dict()

def store_nodedb(k, v):
    print(f'store nodedb entry: {k} {v}')

def del_nodedb_entry(k):
    print(f'delete entry from nodedb: {k}')

def test_lokinet_run():
    pylokinet.set_log_level('trace')
    ctx = Lokinet()
    ctx.nodedb_load = load_nodedb
    ctx.nodedb_store = store_nodedb
    ctx.nodedb_del = del_nodedb_entry
    req = requests.get("https://seed.lokinet.org/lokinet.signed", stream=True)
    ctx.add_bootstrap_rc(req.content)
    print("starting....")
    ctx.start()
    while not ctx.wait_for_ready(100):
        pass
    print(f"we are {ctx.localaddr()}")
