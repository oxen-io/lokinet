#!/usr/bin/env python3
#
# python wsgi application for managing many lokinet instances
#

__doc__ = """lokinet bootserv wsgi app
run me with via gunicorn pylokinet.bootserv:app
"""

import os

from pylokinet import rc
import random

class RCHolder:

    _dir = '/tmp/lokinet_nodedb/'

    _rc_files = list()

    def __init__(self):
        if os.path.exists(self._dir):
            for root, _, files in os.walk(self._dir):
                for f in files:
                    self._add_rc(os.path.join(root, f))
        else:
            os.mkdir(self._dir)
        
    def validate_then_put(self, body):
        if not rc.validate(body):
            return False
        k = rc.get_pubkey(body)
        print(k)
        if k is None:
            return False
        with open(os.path.join(self._dir, k), "wb") as f:
            f.write(body)
        return True

    def _add_rc(self, fpath):
        self._rc_files.append(fpath)

    def serve_random(self):
        with open(random.choice(self._rc_files), 'rb') as f:
            return f.read()

    def empty(self):
        return len(self._rc_files) == 0


def handle_rc_upload(body, respond):
    holder = RCHolder()
    if holder.validate_then_put(body):
        respond("200 OK", [("Content-Type", "text/plain")])
        return ["rc accepted".encode('ascii')]
    else:
        respond("400 Bad Request", [("Content-Type", "text/plain")])
        return ["bad rc".encode('ascii')]
     

def serve_random_rc():
    holder = RCHolder()
    if holder.empty():
        return None
    else:
        return holder.serve_random()

def response(status, msg, respond):
    respond(status, [("Content-Type", "text/plain"), ("Content-Length", "{}".format(len(msg)))])
    return [msg.encode("utf-8")]

def app(environ, start_response):
    request_body_size = int(environ.get("CONTENT_LENGTH", 0))
    method = environ.get("REQUEST_METHOD")
    if method.upper() == "PUT" and request_body_size > 0:
        rcbody = environ.get("wsgi.input").read(request_body_size)
        return handle_rc_upload(rcbody, start_response)
    elif method.upper() == "GET":
        if environ.get("PATH_INFO") == "/bootstrap.signed":
            resp = serve_random_rc()
            if resp is not None:
                start_response('200 OK', [("Content-Type", "application/octet-stream")])
                return [resp]
            else:
                return response('404 Not Found', 'no RCs', start_response)
        elif environ.get("PATH_INFO") == "/ping":
            return response('200 OK', 'pong', start_response)
        else:
            return response('400 Bad Request', 'invalid path', start_response)
    else:
        return response('405 Method Not Allowed', 'method not allowed', start_response)
