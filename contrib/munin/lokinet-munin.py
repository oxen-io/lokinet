#!/usr/bin/env python3
#
# requires python3-requests
#
import requests
import json
import sys


def jsonrpc(method, **args):
    return requests.post('http://127.0.0.1:1190/', data=json.dumps(
        {'method': method, 'params': args, 'id': 0}), headers={'content-type': 'application/json'}).json()


def main():
    if len(sys.argv) == 2 and sys.argv[1] == 'config':
        print("graph_title lokinet peers")
        print("lokinet.peers.outbound Number of outbound lokinet peers")
        print("lokinet.peers.inbound Number of inbound lokinet peers")
    else:
        inbound = 0
        outbound = 0
        try:
            j = jsonrpc("llarp.admin.link.neighboors")
            for peer in j['result']:
                if peer["outbound"]:
                    outbound += 1
                else:
                    inbound += 1
        except:
            pass

        print("lokinet.peers.outboud {}".format(outbound))
        print("lokinet.peers.inboud {}".format(inbound))


if __name__ == '__main__':
    main()
