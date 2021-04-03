#!/usr/bin/env python3

from http.server import ThreadingHTTPServer as Server
from http.server import BaseHTTPRequestHandler as BaseHandler


bootstrapFromURL = True
try:
    import requests
except ImportError:
    bootstrapFromURL = False

import ctypes
from ctypes.util import find_library

import selectors
import socket

import os

class ResultStruct(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("err", ctypes.c_int),
        ("local_address", ctypes.c_char * 256),
        ("local_port", ctypes.c_int),
        ("stream_id", ctypes.c_int)
    ]

    def __repr__(self):
        return "<Result err={} addr={} port={} id={}>".format(self.err, self.local_address, self.local_port, self.stream_id)


class LNContext(ctypes.Structure):
    pass

class Context:
    """
    wrapper around liblokinet
    """

    def __init__(self, debug=False):
        self._ln = ctypes.CDLL(find_library("lokinet"))
        self._c = ctypes.CDLL(find_library("c"))
        self._ln.lokinet_context_new.restype = ctypes.POINTER(LNContext)
        self._ln.lokinet_address.restype = ctypes.c_char_p
        self._ln.lokinet_address.argtypes = (ctypes.POINTER(LNContext), )
        self._ln.lokinet_outbound_stream.restype = ctypes.POINTER(ResultStruct)
        self._ln.lokinet_outbound_stream.argtypes = (ctypes.POINTER(ResultStruct), ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(LNContext))
        self._ctx = self._ln.lokinet_context_new()
        self._addrmap = dict()
        self._debug = debug
        lvl = 'none'
        if self._debug:
            lvl = 'debug'
        self._ln.lokinet_log_level(ctypes.create_string_buffer(lvl.encode('ascii')))


    def free(self, ptr):
        self._c.free(ptr)

    def add_bootstrap(self, data):
        ptr = ctypes.create_string_buffer(data)
        ptrlen = ctypes.c_size_t(len(data))
        return self.ln_call("lokinet_add_bootstrap_rc", ptr, ptrlen)

    def wait_for_ready(self, ms):
        return self.ln_call("lokinet_wait_for_ready", ms) == 0

    def addr(self):
        return self._ln.lokinet_address(self._ctx).decode('ascii')

    def expose(self, port):
        return self.ln_call('lokinet_inbound_stream', port)

    def ln_call(self, funcname, *args):
        args += (self._ctx,)
        if self._debug:
            print("call {}{}".format(funcname, args))
        return self._ln[funcname](*args)

    def expose(self, port):
        port = int(port)
        print("exposing loopback port: {}".format(port))
        return self.ln_call("lokinet_inbound_stream", port)

    def start(self):
        return self.ln_call("lokinet_context_start")

    def stop(self):
        self.ln_call("lokinet_context_stop")

    def hasAddr(self, addr):
        return addr in self._addrmap

    def putAddr(self, addr, val):
        self._addrmap[addr] = val

    def getAddr(self, addr):
        if addr in self._addrmap:
            return self._addrmap[addr]

    def __del__(self):
        self.stop()
        self._ln_call("lokinet_context_free")

    def set_netid(self, netid):
        self._ln.lokinet_set_netid(ctypes.create_string_buffer(netid.encode('ascii')))

class Stream:

    def __init__(self, ctx):
        self._ctx = ctx
        self._id = None

    def connect(self, remote):
        result = ResultStruct()
        self._ctx.ln_call("lokinet_outbound_stream", ctypes.cast(ctypes.addressof(result), ctypes.POINTER(ResultStruct)), ctypes.create_string_buffer(remote.encode()), ctypes.c_char_p(0))

        if result.err:
            print(result.err)
            return
        addr = result.local_address.decode('ascii')
        port = result.local_port
        self._id = result.stream_id
        print("connection to {} made via {}:{} via {}".format(remote, addr, port, self._id))
        return addr, port


    def close(self):
        if self._id is not None:
            self._ctx.ln_call("lokinet_close_stream", self._id)

def read_and_forward_or_close(readfd, writefd):
    read = 0
    while True:
        data = os.read(readfd, 1024)
        read += len(data)
        if data and len(data) > 0:
            writefd.write(data)
            writefd.flush()
            return True
        else:
            return read > 0

ctx = None


class Handler(BaseHandler):

    def do_CONNECT(self):
        self.connect(self.path)

    def connect(self, host):
        global ctx
        if not ctx.hasAddr(host):
            stream = Stream(ctx)

            result = stream.connect(host)
            if not result:
                self.send_error(503)
                return
            ctx.putAddr(host, result)

        sock = socket.socket()
        try:
            sock.connect(ctx.getAddr(host))
        except:
            self.delAddr(host)
            self.send_error(504)
            return

        self.send_response_only(200)
        self.end_headers()
        sel = selectors.DefaultSelector()
        sock.setblocking(False)
        sockfd = sock.makefile('rwb')
        sel.register(self.rfile.fileno(), selectors.EVENT_READ, lambda x : read_and_forward_or_close(x, sockfd))
        sel.register(sock.fileno(), selectors.EVENT_READ, lambda x : read_and_forward_or_close(x, self.wfile))
        while True:
            events = sel.select(1)
            if not events:
                continue
            for key, mask in events:
                if not key.data(key.fileobj):
                    sel.unregister(self.rfile)
                    sel.unregister(sock)
                    sel.close()
                    return

import os
import sys
from argparse import ArgumentParser as AP

ap = AP()
ap.add_argument("--ip", type=str, help="ip to bind to", default="127.0.0.1")
ap.add_argument("--port", type=int, help="port to bind to", default=3000)
ap.add_argument("--expose", type=int, help="expose a port to loopback")
ap.add_argument("--bootstrap", type=str, help="bootstrap file", default="bootstrap.signed")
ap.add_argument("--netid", type=str, help="override network id")
ap.add_argument("--debug", action="store_const", const=True, default=False, help="enable verose logging")
if bootstrapFromURL:
    ap.add_argument("--bootstrap-url", type=str, help="bootstrap from remote url", default="https://seed.lokinet.org/lokinet.signed")

args = ap.parse_args()
addr = (args.ip, args.port)
server = Server(addr, Handler)

ctx = Context(args.debug)

if args.netid is not None:
    print("overriding netid: {}".format(args.netid))
    ctx.set_netid(args.netid)

if os.path.exists(args.bootstrap):
    with open(args.bootstrap, 'rb') as f:
        if ctx.add_bootstrap(f.read()) == 0:
            print("loaded {}".format(args.bootstrap))

if args.bootstrap_url is not None:
    print('getting bootstrap info from {}'.format(args.bootstrap_url))
    resp = requests.get(args.bootstrap_url)
    if resp.status_code == 200 and ctx.add_bootstrap(resp.content) == 0:
        pass
    else:
        print("failed")

print("starting up...")
if ctx.start() != 0:
    print("failed to start")
    ctx.stop()
    sys.exit(-1)

id = None

try:
    while not ctx.wait_for_ready(500):
        print("waiting for lokinet...")
    lokiaddr = ctx.addr()
    print("we are {}".format(lokiaddr))
    if args.expose:
        id = ctx.expose(args.expose)
        print("exposed {}:{}".format(lokiaddr, args.expose))
    print("serving on {}:{}".format(*addr))
    server.serve_forever()
finally:
    if id is not None:
        ctx.ln_call("lokinet_close_stream", id)
    ctx.stop()
