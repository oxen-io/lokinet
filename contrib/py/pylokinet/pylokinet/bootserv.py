#!/usr/bin/env python3
#
# python wsgi application for managing many lokinet instances
#

__doc__ = """lokinet bootserv wsgi app
also handles webhooks for CI
run me with via gunicorn pylokinet.bootserv:app
"""

import os

from pylokinet import rc
import json 

import random
import time
from datetime import datetime
from email.utils import parsedate, format_datetime
from dateutil.parser import parse as date_parse
import requests


root = './lokinet'

def _compare_dates(left, right):
    """
    return true if left timestamp is bigger than right
    """
    return date_parse(left) > date_parse(right)

class TokenHolder:

    _dir = root
    _token = None

    def __init__(self, f="token"):
        if not os.path.exists(self._dir):
            os.mkdir(self._dir, 0o700)
        f = os.path.join(self._dir, f)
        if os.path.exists(f):
            with open(f) as fd:
                self._token = fd.read().replace("\n", "")

    def verify(self, token):
        """
        return true if token matches
        """
        if self._token is None:
            return False
        return self._token == token

class BinHolder:
    """
    serves a binary file in a dir
    """
    _dir = os.path.join(root, 'bin')

    def __init__(self, f):
        if not os.path.exists(self._dir):
            os.mkdir(self._dir, 0o700)
        self._fpath = os.path.join(self._dir, f)
            
    def put(self, r):
        """
        put a new file into the place that is held
        """
        with open(self._fpath, "wb") as f:
            for chunk in r.iter_content(chunk_size=1024):
                f.write(chunk)


    def is_new(self, date):
        """
        return true if last modified timestamp is fresher than current
        """
        t = date_parse('{}'.format(date))
        if not t:
            return False
        if os.path.exists(self._fpath):
            st = os.stat(self._fpath)
            return st.st_mtime >= t.timestamp()
        return True


    def serve(self, last_modified, respond):
        """
        serve file with caching
        """
        t = parsedate(last_modified)
        if t:
            t = time.mktime(t)
        if t is None:
            t = 0
        if not os.path.exists(self._fpath):
            respond("404 Not Found", [])
            return []
        st = os.stat(self._fpath)
        if st.st_mtime < t:
            respond("304 Not Modified", [("Last-Modified", format_datetime(st.st_mtime)) ])
            return []
        with open(self._fpath, "rb") as f:
            data = f.read()
        respond("200 OK", [("Content-Type", "application/octect-stream"), 
        ("Last-Modified", format_datetime(datetime.fromtimestamp(int(st.st_mtime)))),("Content-Length", "{}".format(st.st_size))])
        return [data]


class RCHolder:

    _dir = os.path.join(root, 'nodedb')

    _rc_files = list()

    def __init__(self):
        if os.path.exists(self._dir):
            for root, _, files in os.walk(self._dir):
                for f in files:
                    self._add_rc(os.path.join(root, f))
        else:
            os.mkdir(self._dir, 0o700)
        
    def prune(self):
        """
        remove invalid entries
        """
        delfiles = []
        for p in self._rc_files:
            with open(p, 'rb') as f:
                if not rc.validate(f.read()):
                    delfiles.append(p)
        for f in delfiles:
            os.remove(f)

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

def handle_serve_lokinet(modified_since, respond):
    l = BinHolder('lokinet.zip')
    return l.serve(modified_since, respond)


def fetch_lokinet(j):
    holder = BinHolder("lokinet.zip")
    if 'builds' not in j:
        return False
    selected = None
    for build in j['builds']:
        if 'finished_at' not in build or build['finished_at'] is None:
            continue
        if holder.is_new(build['finished_at']):
            if selected is None or _compare_dates(build["finished_at"], selected["finished_at"]):
                selected = build
    if selected and 'id' in selected:
        url = 'https://gitlab.com/lokiproject/loki-network/-/jobs/{}/artifacts/download'.format(selected['id'])
        r = requests.get(url)
        if r.status_code == 200:
            holder.put(r)
    return True

    #if 'artifacts_file' not in selected:
    #    return False
    #f = selected["artifacts_file"]
    #return True

def handle_webhook(j, token, event, respond):
    """
    handle CI webhook
    """
    t = TokenHolder()
    if not t.verify(token):
        respond("403 Forbidden", [])
        return []
    event = event.lower()
    if event == 'pipeline hook':
        if fetch_lokinet(j):
            respond("200 OK", [])
            return []
        else:
            respond("500 Internal Server Error", [])
            return []
    else:
        respond("404 Not Found", [])
        return []


def app(environ, start_response):
    request_body_size = int(environ.get("CONTENT_LENGTH", 0))
    method = environ.get("REQUEST_METHOD")
    if method.upper() == "PUT" and request_body_size > 0:
        rcbody = environ.get("wsgi.input").read(request_body_size)
        return handle_rc_upload(rcbody, start_response)
    elif method.upper() == "POST":
        if environ.get("PATH_INFO") == "/":
            j = json.loads(environ.get("wsgi.input").read(request_body_size))
            token = environ.get("HTTP_X_GITLAB_TOKEN")
            return handle_webhook(j, token, environ.get("HTTP_X_GITLAB_EVENT"), start_response)
        else:
            return response("404 Not Found", 'bad url', start_response)
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
        elif environ.get("PATH_INFO") == "/lokinet.zip":
            return handle_serve_lokinet(environ.get("HTTP_IF_MODIFIED_SINCE"),start_response)
        else:
            return response('400 Bad Request', 'invalid path', start_response)
    else:
        return response('405 Method Not Allowed', 'method not allowed', start_response)


def main():
    """
    run as cron job
    """
    h = RCHolder()
    h.prune()

if __name__ == '__main__':
    main()
