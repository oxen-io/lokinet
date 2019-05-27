#!/usr/bin/env python3
#
# requires python3-requests
#
import requests
import json
import os
import sys

from collections import defaultdict as Dict

from requests.exceptions import RequestException


def jsonrpc(method, **args):
    return requests.post('http://127.0.0.1:1190/', data=json.dumps(
        {'method': method, 'params': args, 'id': 'munin'}), headers={'content-type': 'application/json'}).json()


def exit_sessions_main(exe):
    if len(sys.argv) == 2 and sys.argv[1] == 'config':
        print("graph_title lokinet exit sessions")
        print("graph_vlabel sessions")
        print("graph_category network")
        print("graph_info This graph shows the number of exit sessions on a lokinet exit")
        print("{}.sessions.info Number of exit sessions".format(exe))
        print("{}.sessions.label sessions".format(exe))
    else:
        count = 0
        try:
            j = jsonrpc("llarp.admin.exit.list")
            count = len(j['result'])
        except RequestException:
            pass
        print("{}.sessions {}".format(exe, count))


def peers_main(exe):
    if len(sys.argv) == 2 and sys.argv[1] == 'config':
        print("graph_title lokinet peers")
        print("graph_vlabel peers")
        print("graph_category network")
        print("graph_info This graph shows the number of node to node sessions of this lokinet router")
        print("{}.outbound.info Number of outbound lokinet peers".format(exe))
        print("{}.inbound.info Number of inbound lokinet peers".format(exe))
        print("{}.outbound.label outbound peers".format(exe))
        print("{}.inbound.label inbound peers".format(exe))
    else:
        inbound = Dict(int)
        outbound = Dict(int)
        try:
            j = jsonrpc("llarp.admin.link.neighboors")
            for peer in j['result']:
                if peer["outbound"]:
                    outbound[peer['ident']] += 1
                else:
                    inbound[peer['ident']] += 1
        except RequestException:
            pass

        print("{}.outbound {}".format(exe, len(outbound)))
        print("{}.inbound {}".format(exe, len(inbound)))


if __name__ == '__main__':
    exe = os.path.basename(sys.argv[0]).lower()
    if exe == 'lokinet_peers':
        peers_main(exe)
    elif exe == 'lokinet_exit':
        exit_sessions_main(exe)
    else:
        print(
            'please symlink this as `lokinet_peers` or `lokinet_exit` in munin plugins dir')
