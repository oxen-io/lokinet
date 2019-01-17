#
# super freaking dead simple wicked awesome bencode library
#
from io import BytesIO

class BCodec:
  encoding = 'utf-8'
  def __init__(self, fd):
    self._fd = fd

  def _write_bytestring(self, bs):
    self._fd.write('{}:'.format(len(bs)).encode('ascii'))
    self._fd.write(bs)
  
  def _write_list(self, l):
    self._fd.write(b'l')
    for item in l:
      self.encode(item)
    self._fd.write(b'e')

  def _write_dict(self, d):
    self._fd.write(b'd')
    keys = list(d.keys())
    keys.sort()
    for k in keys:
      if isinstance(k, str):
        self._write_bytestring(k.encode(self.encoding))
      elif isinstance(k, bytes):
        self._write_bytestring(k)
      else:
        self._write_bytestring('{}'.format(k).encode(self.encoding))
      self.encode(d[k])
    self._fd.write(b'e')

  def _write_int(self, i):
    self._fd.write('i{}e'.format(i).encode(self.encoding))

  def encode(self, obj):
    if isinstance(obj, dict):
      self._write_dict(obj)
    elif isinstance(obj, list):
      self._write_list(obj)
    elif isinstance(obj, int):
      self._write_int(obj)
    elif isinstance(obj, str):
      self._write_bytestring(obj.encode(self.encoding))
    elif isinstance(obj, bytes):
      self._write_bytestring(obj)
    elif hasattr(obj, bencode):
      obj.bencode(self._fd)
    else:
      raise ValueError("invalid object type")

  def _readuntil(self, delim):
    b = bytes()
    while True:
      ch = self._fd.read(1)
      if ch == delim:
        return b
      b += ch

  def _decode_list(self):
    l = list()
    while True:
      b = self._fd.read(1)
      if b == b'e':
        return l
      l.append(self._decode(b))

  def _decode_dict(self):
    d = dict()
    while True:
      ch = self._fd.read(1)
      if ch == b'e':
        return d
      k = self._decode_bytestring(ch)
      d[k] = self.decode()

  def _decode_int(self):
    return int(self._readuntil(b'e'), 10)

  def _decode_bytestring(self, ch):
    ch += self._readuntil(b':')
    l = int(ch, base=10)
    return self._fd.read(l)

  def _decode(self, ch):
    if ch == b'd':
      return self._decode_dict()
    elif ch == b'l':
      return self._decode_list()
    elif ch == b'i':
      return self._decode_int()
    elif ch in [b'0',b'1',b'2',b'3',b'4',b'5',b'6',b'7',b'8',b'9']:
      return self._decode_bytestring(ch)
    else:
      raise ValueError(ch)

  def decode(self):
    return self._decode(self._fd.read(1))


def bencode(obj):
  buf = BytesIO()
  b = BCodec(buf)
  b.encode(obj)
  return buf.bytes()

def bdecode(bytestring):
  buf = BytesIO()
  buf.write(bytestring)
  return BCodec(buf).decode()