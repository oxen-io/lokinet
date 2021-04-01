#!/usr/bin/env python3

from http.server import ThreadingHTTPServer as Server
from http.server import BaseHTTPRequestHandler as BaseHandler

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

    def __init__(self):
        self._ln = ctypes.CDLL(find_library("lokinet"))
        self._c = ctypes.CDLL(find_library("c"))
        self._ln.lokinet_context_new.restype = ctypes.POINTER(LNContext)
        self._ln.lokinet_address.restype = ctypes.c_char_p
        self._ln.lokinet_address.argtypes = (ctypes.POINTER(LNContext), )
        self._ln.lokinet_outbound_stream.restype = ctypes.POINTER(ResultStruct)
        self._ln.lokinet_outbound_stream.argtypes = (ctypes.POINTER(ResultStruct), ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(LNContext))  
        self._ctx = self._ln.lokinet_context_new()

    def free(self, ptr):
        self._c.free(ptr)


    def addr(self):
        return self._ln.lokinet_address(self._ctx).decode('ascii')

    def expose(self, port):
        return self.ln_call('lokinet_inbound_stream', port)
    
    def ln_call(self, funcname, *args):
        args += (self._ctx,)
        print("call {}{}".format(funcname, args))
        return self._ln[funcname](*args)
        
    def start(self):
        self.ln_call("lokinet_context_start")

    def stop(self):
        self.ln_call("lokinet_context_stop")

    def __del__(self):
        self.stop()
        self._ln_call("lokinet_context_free")    

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
        print("connect to {} made via {}:{} via {}".format(remote, addr, port, self._id))
        return addr, port


    def close(self):
        if self._id is not None:
            self._ctx.ln_call("lokinet_close_stream", self._id)

def read_and_forward_or_close(readfd, writefd):
    read = 0
    while True:
        data = os.read(readfd, 128)
        read += len(data)
        if data and len(data) > 0:
            writefd.write(data)
            writefd.flush()
        else:
            return read > 0
        
ctx = Context()

        
class Handler(BaseHandler):

    def do_CONNECT(self):
        self.connect(self.path)
    
    def connect(self, host):
        global ctx
        stream = Stream(ctx)

        result = stream.connect(host)
        if not result:
            self.send_error(503)
            return
        sock = socket.socket()
        sock.connect(result)
        if not sock:
            self.send_error(504)
            return
        
        self.send_response_only(200)
        self.end_headers()
        sel = selectors.DefaultSelector()
        sock.setblocking(False)
        sockfd = sock.makefile('rwb')
        sel.register(self.rfile.fileno(), selectors.EVENT_READ, lambda x : read_and_forward_or_close(x, sockfd))
        sel.register(sock.fileno(), selectors.EVENT_READ, lambda x : read_and_forward_or_close(x, self.wfile))

        print("running")
        while True:
            events = sel.select(1)
            if not events:
                continue
            for key, mask in events:
                if not key.data(key.fileobj):
                    sel.unregister(self.rfile)
                    sel.unregister(sock)
                    sel.close()
                    stream.close()
                    return


server = Server(('127.0.0.1', 3000), Handler)
ctx.start()
id = ctx.expose(80)
print("we are {}".format(ctx.addr()))
try:
    server.serve_forever()
except:
    ctx.ln_call("lokinet_close_stream", id)
    ctx.stop()
