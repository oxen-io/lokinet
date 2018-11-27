#!/usr/bin/env python3
import bencode
import sys
import libnacl
import base64
import struct
from io import BytesIO
import time
from multiprocessing import Process, Array, Value



def print_help():
  print('usage: {} keyfile.private prefix numthreads'.format(sys.argv[0]))
  return 1

_zalpha = ['y', 'b', 'n', 'd', 'r', 'f', 'g', '8',
  'e', 'j', 'k', 'm', 'c', 'p', 'q', 'x',
  'o', 't', '1', 'u', 'w', 'i', 's', 'z',
  'a', '3', '4', '5', 'h', '7', '6', '9']

def zb32_encode(buf):
  s = str()
  bits = 0
  l = len(buf)
  idx = 0 
  tmp = buf[idx]
  while bits > 0 or idx < l:
    if bits < 5:
      if idx < l:
        tmp <<= 8
        tmp |= buf[idx] & 0xff
        idx += 1
        bits += 8
      else:
        tmp <<= 5 - bits
        bits = 5
    bits -= 5
    s += _zalpha[(tmp >> bits) & 0x1f]
  return s


def _gen_si(keys):
  e = keys[b'e'][32:]
  s = keys[b's'][32:]
  v = keys[b'v']
  return {'e': e, 's':s, 'v':v}


class AddrGen:

  def __init__(self, threads, keys, prefix):
    self._inc = threads
    self._keys = keys
    self._c = Value('i')
    self.sync = Array('i', 3)
    self._procs = []
    self.prefix = prefix

  def runit(self):
    for ch in self.prefix:
      if ch not in _zalpha:
        print("invalid prefix, {} not a valid character".format(ch))
        return None, None
    print("find ^{}.loki".format(self.prefix))
    i = self._inc
    while i > 0:
      p = Process(target=self._gen_addr_tick, args=(self.prefix, abs(libnacl.randombytes_random()), abs(libnacl.randombytes_random()), _gen_si(self._keys)))
      p.start()
      self._procs.append(p)
      i -=1
    return self._runner()

  def _gen_addr_tick(self, prefix, lo, hi, si):
    print(prefix)
    fd = BytesIO()
    addr = ''
    enc = bencode.BCodec(fd)
    while self.sync[2] == 0:
      si['x'] = struct.pack('>QQ', lo, hi)
      fd.seek(0,0)
      enc.encode(si)
      pub = bytes(fd.getbuffer())
      addr = zb32_encode(libnacl.crypto_generichash(pub))
      if addr.startswith(prefix):
        print(addr)
        self.sync[2] = 1 
        self.sync[0] = hi
        self.sync[1] = lo
        return
      hi += self._inc
      if hi == 0:
        lo += 1
      self._c.value += 1

  def _print_stats(self):
    print('{} H/s'.format(self._c.value))
    self._c.value = 0

  def _joinall(self):
    for p in self._procs:
      p.join()

  def _runner(self):
      while self.sync[2] == 0:
        time.sleep(1)
        self._print_stats()
      self._joinall()
      fd = BytesIO()
      enc = bencode.BCodec(fd)
      hi = self.sync[0]
      lo = self.sync[1]
      si = _gen_si(self._keys)
      si['x'] = struct.pack('>QQ', lo, hi)
      enc.encode(si)
      pub = bytes(fd.getbuffer())
      addr = zb32_encode(libnacl.crypto_generichash(pub))
      return si['x'], addr
  

def main(args):
  if len(args) != 3:
    return print_help()
  keys = None
  with open(args[0], 'rb') as fd:
    dec = bencode.BCodec(fd)
    keys = dec.decode()
  runner = AddrGen(int(args[2]), keys, args[1])
  keys[b'x'], addr = runner.runit()
  if addr:
    print("found {}.loki".format(addr))
    with open(args[0], 'wb') as fd:
      enc = bencode.BCodec(fd)
      enc.encode(keys)

if __name__ == '__main__':
  main(sys.argv[1:])