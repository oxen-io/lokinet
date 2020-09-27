#!/usr/bin/env python3
"""
keygen tool for lokinet
"""

from argparse import ArgumentParser as AP
from base64 import b32encode

import pysodium

def base32z(data):
    """ base32 z encode """
    return b32encode(data).translate(
        bytes.maketrans(
            b'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567',
            b'ybndrfg8ejkmcpqxot1uwisza345h769')).decode().rstrip('=')


def main():
    """
    main function for keygen
    """
    argparser = AP()
    argparser.add_argument('--keyfile', type=str, required=True, help='place to put generated keys')
    args = argparser.parse_args()
    keys = pysodium.crypto_sign_keypair()
    with open(args.keyfile, 'wb') as wfile:
        wfile.write('d1:s{}:'.format(len(keys[1])).encode('ascii'))
        wfile.write(keys[1])
        wfile.write(b'e')
    print("{}.loki".format(base32z(keys[0])))

if __name__ == '__main__':
    main()
