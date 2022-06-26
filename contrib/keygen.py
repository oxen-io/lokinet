#!/usr/bin/env python3
#
# .loki secret key generator script
# makes keyfile contents
#
# usage: python3 keygen.py out.private
#        python3 keygen.py > /some/where/over/the/rainbow
#
from nacl.bindings import crypto_sign_keypair
import sys

out = sys.stdout

close_out = lambda : None
args = sys.argv[1:]

if args and args[0] != '-':
  out = open(args[0], 'wb')
  close_out = out.close

pk, sk = crypto_sign_keypair()
out.write(b'64:')
out.write(sk)
out.flush()
close_out()

