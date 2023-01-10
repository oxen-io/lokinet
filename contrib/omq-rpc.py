#!/usr/bin/env python3

import nacl.bindings as sodium
from nacl.public import PrivateKey
from nacl.signing import SigningKey, VerifyKey
import nacl.encoding
import requests
import zmq
import zmq.utils.z85
import sys
import re
import time
import random
import shutil


context = zmq.Context()
socket = context.socket(zmq.DEALER)
socket.setsockopt(zmq.CONNECT_TIMEOUT, 5000)
socket.setsockopt(zmq.HANDSHAKE_IVL, 5000)
#socket.setsockopt(zmq.IMMEDIATE, 1)

if len(sys.argv) > 1 and any(sys.argv[1].startswith(x) for x in ("ipc://", "tcp://", "curve://")):
    remote = sys.argv[1]
    del sys.argv[1]
else:
    remote = "ipc://./rpc.sock"

curve_pubkey = b''
my_privkey, my_pubkey = b'', b''

# If given a curve://whatever/pubkey argument then transform it into 'tcp://whatever' and put the
# 'pubkey' back into argv to be handled below.
if remote.startswith("curve://"):
    pos = remote.rfind('/')
    pkhex = remote[pos+1:]
    remote = "tcp://" + remote[8:pos]
    if len(pkhex) != 64 or not all(x in "0123456789abcdefABCDEF" for x in pkhex):
        print("curve:// addresses must be in the form curve://HOST:PORT/REMOTE_PUBKEY_HEX", file=sys.stderr)
        sys.exit(1)
    sys.argv[1:0] = [pkhex]

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

if not 2 <= len(sys.argv) <= 3 or any(x in y for x in ("--help", "-h") for y in sys.argv[1:]):
    print("Usage: {} [ipc:///path/to/sock|tcp://1.2.3.4:5678] [SERVER_CURVE_PUBKEY [LOCAL_CURVE_PRIVKEY]] COMMAND ['JSON']".format(
        sys.argv[0]), file=sys.stderr)
    sys.exit(1)

beginning_of_time = time.clock_gettime(time.CLOCK_MONOTONIC)

print("Connecting to {}".format(remote), file=sys.stderr)
socket.connect(remote)
to_send = [sys.argv[1].encode(), b'tagxyz123']
to_send += (x.encode() for x in sys.argv[2:])
print("Sending {}".format(to_send[0]), file=sys.stderr)
socket.send_multipart(to_send)
if socket.poll(timeout=5000):
    m = socket.recv_multipart()
    recv_time = time.clock_gettime(time.CLOCK_MONOTONIC)
    if len(m) < 3 or m[0:2] != [b'REPLY', b'tagxyz123']:
        print("Received unexpected {}-part reply:".format(len(m)), file=sys.stderr)
        for x in m:
            print("- {}".format(x))
    else: # m[2] is numeric value, m[3] is data part, and will become m[2] <- changed
        print("Received reply in {:.6f}s:".format(recv_time - beginning_of_time), file=sys.stderr)
        if len(m) < 3:
            print("(empty reply data)", file=sys.stderr)
        else:
            for x in m[2:]:
                print("{} bytes data part:".format(len(x)), file=sys.stderr)
                if any(x.startswith(y) for y in (b'd', b'l', b'i')) and x.endswith(b'e'):
                    sys.stdout.buffer.write(x)
                else:
                    print(x.decode(), end="\n\n")

else:
    print("Request timed out", file=sys.stderr)
    socket.close(linger=0)
    sys.exit(1)


# ./lmq-rpc.py ipc://$HOME/.oxen/testnet/oxend.sock 'llarp.get_service_nodes' | jq