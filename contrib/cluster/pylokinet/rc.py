from lokinet import bencode
import pysodium

def validate(data):
  rc = bencode.bdecode(data)
  if 'z' not in rc or 'k' not in rc:
    return False
  sig = rc['z']
  rc['z'] = '\x00' * 32
  buf = bencode.bencode(rc)
  try:
    pysodium.crypto_sign_verify_detached(sig, buf, rc['k'])
  except:
    return False
  else:
    return True

def get_pubkey(data):
   rc = bencode.bdecode(data)
   if 'k' in rc:
     return rc['k'].encode('hex')