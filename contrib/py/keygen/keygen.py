#!/usr/bin/env python3
"""
keygen tool for lokinet
"""

from argparse import ArgumentParser as AP
from base64 import b32encode

from nacl.signing import SigningKey

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
    secret = SigningKey.generate()
    with open(args.keyfile, 'wb') as wfile:
        wfile.write(b'd1:s64:')
        wfile.write(secret.encode())
        wfile.write(secret.verify_key.encode())
        wfile.write(b'e')
    print("{}.loki".format(base32z(secret.verify_key.encode())))

if __name__ == '__main__':
    main()
