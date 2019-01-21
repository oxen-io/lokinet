from pylokinet import bencode
import pysodium
import binascii

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
    return True

def get_pubkey(data):
   rc = bencode.bdecode(data)
   if b'k' in rc:
     return binascii.hexlify(rc[b'k']).decode('ascii')