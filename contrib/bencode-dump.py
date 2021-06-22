#!/usr/bin/python3

import sys
import pprint

if len(sys.argv) != 2 or sys.argv[1].startswith('-'):
    print("Usage: {} FILE -- dumps a bencoded file".format(sys.argv[0]), file=sys.stderr)
    sys.exit(1)

f = open(sys.argv[1], 'rb')

class HexPrinter():
    def __init__(self, data):
        self.data = data

    def __repr__(self):
        return "hex({} bytes):'{}'".format(len(self.data), self.data.hex())


def next_byte():
    b = f.read(1)
    assert b is not None and len(b) == 1
    return b


def parse_int():
    s = b''
    x = next_byte()
    while x in b"0123456789":
        s += x
        x = next_byte()
    assert x == b'e' and len(s) > 0, "Invalid integer encoding"
    return int(s)


def parse_string(s):
    x = next_byte()
    while x in b"0123456789":
        s += x
        x = next_byte()
    assert x == b':', "Invalid string encoding"
    s = int(s)
    data = f.read(s)
    assert len(data) == s, "Truncated string data"
    # If the string is ascii then convert to string:
    if all(0x20 <= b <= 0x7e for b in data):
        return data.decode()
    # Otherwise display as hex:
    return HexPrinter(data)


def parse_dict():
    d = {}
    last_key = None
    while True:
        t = next_byte()
        if t == b'e':
            return d
        assert t in b"0123456789", "Invalid dict: dict keys must be strings"
        key = parse_string(t)
        raw_key = key.data if isinstance(key, HexPrinter) else key.encode()
        if last_key is not None and raw_key <= last_key:
            print("Warning: found out-of-order dict keys ({} after {})".format(raw_key, last_key), file=sys.stderr)
        last_key = raw_key
        t = next_byte()
        d[key] = parse_thing(t)


def parse_list():
    l = []
    while True:
        t = next_byte()
        if t == b'e':
            return l
        l.append(parse_thing(t))


def parse_thing(t):
    if t == b'd':
        return parse_dict()
    if t == b'l':
        return parse_list()
    if t == b'i':
        return parse_int()
    if t in b"0123456789":
        return parse_string(t)
    assert False, "Parsing error: encountered invalid type '{}'".format(t)


pprint.PrettyPrinter(
        indent=2
        ).pprint(parse_thing(next_byte()))
