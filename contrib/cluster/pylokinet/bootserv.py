#!/usr/bin/env python3
#
# python wsgi application for managing many lokinet instances
#

__doc__ = """lokinet bootserv wsgi app
run me with via gunicorn pylokinet.bootserv:app
"""

import os

from lokinet import rc
import random

class RCHolder:

    _dir = '/tmp/lokinet_nodedb'

    _rc_files = list()

    def __init__(self):
        if os.path.exists(self._dir):
            os.path.walk(self._dir, lambda _, _, f : self._load_subdir(f), None)
        else:
            os.mkdir(self._dir)
        
    def validate_then_put(self, body):
        if not rc.validate(body):
            return False
        k = rc.get_pubkey(body)
        if k is None:
            return False
        with open(os.path.join(self._dir, k), "wb") as f:
            f.write(body)
        return True

    def _add_rc(self, fpath):
        self._rc_files.append(fpath)

    def _load_subdir(self, files):
        for f in files:
            path = os.path.join(self._dir, f)
            self._add_rc(path)

    def serve_random(self):
        with open(random.choice(self._rc_files), 'rb') as f:
            return f.read()

    def empty(self):
        return len(self._rc_files) == 0


def bad_request(msg):
    return "400 Bad Request", [("Content-Type", "text/plain"), ['{}'.format(msg)]]

def status_ok(msg):
    return "200 OK", [("Content-Type", "text/plain"), ['{}'.format(msg)]]

def handle_rc_upload(body):
    holder = RCHolder()
    if not holder.validate_then_put(body):
        return bad_request('invalid rc')
    return status_ok("rc accepted")

def serve_random_rc():
    holder = RCHolder()
    if holder.empty():
        return '404 Not Found', [("Content-Type", "application/octect-stream")]
    else:
        return '200 OK', [("Content-Type", "application/octect-stream"), [holder.serve_random()]]

def app(environ, start_response):
    request_body_size = int(environ.get("CONTENT_LENGTH", 0))
    method = environ.get("REQUEST_METHOD")
    if method.upper() == "PUT" and request_body_size > 0:
        rcbody = environ.get("wsgi.input").read(request_body_size)
        return handle_rc_upload(rcbody)
    elif method.upper() == "GET":
        if environ.get("PATH_INFO") == "/bootstrap.signed":
            return serve_random_rc()
        elif environ.get("PATH_INFO") == "/ping":
            return status_ok("pong")
        else:
            bad_request("bad path")
    else:
        return bad_request("invalid request")
