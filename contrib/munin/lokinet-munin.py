#!/usr/bin/env python3
#
# requires python3-requests
#
import requests
import json
import sys

from collections import defaultdict as Dict

from requests.exceptions import RequestException


def jsonrpc(method, **args):
    return requests.post('http://127.0.0.1:1190/', data=json.dumps(
        {'method': method, 'params': args, 'id': 'munin'}), headers={'content-type': 'application/json'}).json()


def exit_sessions_main():
    if len(sys.argv) == 2 and sys.argv[1] == 'config':
        print("graph_title lokinet exit sessions")
        print("graph_vlabel sessions")
        print("graph_category network")
        print("graph_info This graph shows the number of exit sessions on a lokinet exit")
        print("lokinet.exit.sessions.info Number of exit sessions")
        print("lokinet.exit.sessions.label sessions")
    else:
        count = 0
        try:
            j = jsonrpc("llarp.admin.exit.list")
            count = len(j['result'])
        except RequestException:
            pass
        print("lokinet.exit.sessions {}".format(count))


def peers_main():
    if len(sys.argv) == 2 and sys.argv[1] == 'config':
        print("graph_title lokinet peers")
        print("graph_vlabel peers")
        print("graph_category network")
        print("graph_info This graph shows the number of node to node sessions of this lokinet router")
        print("lokinet.peers.outbound.info Number of outbound lokinet peers")
        print("lokinet.peers.inbound.info Number of inbound lokinet peers")
        print("lokinet.peers.outbound.label outbound peers")
        print("lokinet.peers.inbound.label inbound peers")
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

        print("lokinet.peers.outbound {}".format(len(outbound)))
        print("lokinet.peers.inbound {}".format(len(inbound)))


if __name__ == '__main__':
    if sys.argv[0] == 'lokinet-peers':
        peers_main()
    elif sys.argv[0] == 'lokinet-exit':
        exit_sessions_main()
    else:
        print(
            'please symlink this as `lokinet-peers` or `lokinet-exit` in munin plugins dir')
