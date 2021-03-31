#!/usr/bin/env python3

import nacl.bindings as sodium
from nacl.public import PrivateKey
from nacl.signing import SigningKey, VerifyKey
import nacl.encoding
import requests
import zmq
import zmq.utils.z85
import sys
import os
import re
import time
import random
import shutil

import json

context = zmq.Context()
socket = context.socket(zmq.DEALER)
socket.setsockopt(zmq.CONNECT_TIMEOUT, 5000)
socket.setsockopt(zmq.HANDSHAKE_IVL, 5000)
#socket.setsockopt(zmq.IMMEDIATE, 1)

if len(sys.argv) > 1 and any(sys.argv[1].startswith(x) for x in ("ipc://", "tcp://")):
    remote = sys.argv[1]
    del sys.argv[1]
else:
    remote = "ipc://./loki.sock"

curve_pubkey = b''
my_privkey, my_pubkey = b'', b''
if len(sys.argv) > 1 and len(sys.argv[1]) == 64 and all(x in "0123456789abcdefABCDEF" for x in sys.argv[1]):
    curve_pubkey = bytes.fromhex(sys.argv[1])
    del sys.argv[1]
    socket.curve_serverkey = curve_pubkey
    if len(sys.argv) > 1 and len(sys.argv[1]) == 64 and all(x in "0123456789abcdefABCDEF" for x in sys.argv[1]):
        my_privkey = bytes.fromhex(sys.argv[1])
        del sys.argv[1]
        my_pubkey = zmq.utils.z85.decode(zmq.curve_public(zmq.utils.z85.encode(my_privkey)))
    else:
        my_privkey = PrivateKey.generate()
        my_pubkey = my_privkey.public_key.encode()
        my_privkey = my_privkey.encode()

        print("No curve client privkey given; generated a random one (pubkey: {}, privkey: {})".format(
            my_pubkey.hex(), my_privkey.hex()), file=sys.stderr)
    socket.curve_secretkey = my_privkey
    socket.curve_publickey = my_pubkey

if not 2 <= len(sys.argv) or any(x in y for x in ("--help", "-h") for y in sys.argv[1:]):
    print("Usage: {} [ipc:///path/to/sock|tcp://1.2.3.4:5678] [connect|listen] host port".format(
        sys.argv[0]), file=sys.stderr)
    sys.exit(1)

action = sys.argv[1].lower()
host = sys.argv[2]
port = int(sys.argv[3])
request_path = len(sys.argv) >= 5 and sys.argv[4] or '/'

beginning_of_time = time.clock_gettime(time.CLOCK_MONOTONIC)

#print("Connecting to {}".format(remote), file=sys.stderr)
socket.connect(remote)


def rpc(method, args, timeout=15000):
    to_send = [method.encode(), b'tagxyz123']
    to_send += (x.encode() for x in [json.dumps(args)])
    #print("Sending {}".format(to_send[0]), file=sys.stderr)
    socket.send_multipart(to_send)
    if socket.poll(timeout=timeout):
        m = socket.recv_multipart()
        recv_time = time.clock_gettime(time.CLOCK_MONOTONIC)
        if len(m) < 3 or m[0:2] != [b'REPLY', b'tagxyz123']:
            pass
        else:
            return json.loads(m[2].decode())


args = {"host":host, "port":port}

def success_or_die(response):
    if response:
        if 'error' in response and response['error']:
            print("error: {}".format(response['error']))
            socket.close(linger=0)
            sys.exit(1)
    if response and 'result' in response:
        return response["result"]
    else:
        print("no response")
        socket.close(linger=0)
        sys.exit(1)

if action == "connect":
    result = success_or_die(rpc("llarp.quic_connect", args))
    print(result)
    cmd = "curl -vv http://{}{} -o /dev/null".format(result["addr"], request_path)
    print("{}".format(cmd))
    os.system(cmd)
if action == "listen":
    result = success_or_die(rpc("llarp.quic_listener", args))
    print("ID={} addr={}".format(result["id"], result["addr"]))
