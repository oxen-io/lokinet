from pylokinet import bencode
import pysodium
import binascii
import time

def _expired(ts, lifetime=84600000):
  """
  return True if a timestamp is considered expired
  lifetime is default 23.5 hours
  """
  return (int(time.time()) * 1000) - ts >= lifetime

def validate(data):
  rc = bencode.bdecode(data)
  if b'z' not in rc or b'k' not in rc:
    return False
  sig = rc[b'z']
  rc[b'z'] = b'\x00' * 64
  buf = bencode.bencode(rc)
  try:
    k = rc[b'k']
    pysodium.crypto_sign_verify_detached(sig, buf, k)
  except:
    return False
  else:
    return not _expired(rc[b't'])

def get_pubkey(data):
   rc = bencode.bdecode(data)
   if b'k' in rc:
     return binascii.hexlify(rc[b'k']).decode('ascii')