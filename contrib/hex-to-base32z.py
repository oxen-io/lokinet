#!/usr/bin/python3

import sys

base32z_dict = 'ybndrfg8ejkmcpqxot1uwisza345h769'

def lokinet_snode_addr(pk_hex):
    """Returns the lokinet snode address from a hex ed25519 pubkey"""
    assert(len(pk_hex) == 64)
    bits = 0
    val = 0
    result = ''
    for x in pk_hex:
        bits += 4
        val = (val << 4) + int(x, 16)
        if bits >= 5:
            bits -= 5
            v = val >> bits
            val &= (1 << bits) - 1
            result += base32z_dict[v]
    result += base32z_dict[val << (5 - bits)]
    return result + ".snode"

if len(sys.argv) < 2 or any(len(x) != 64 for x in sys.argv[1:]):
    print("Usage: {} PUBKEY [PUBKEY ...] -- converts ed25519 pubkeys to .snode addresses".format(sys.argv[0]))
    sys.exit(1)

for key in sys.argv[1:]:
    print("{} -> {}".format(key, lokinet_snode_addr(key)))

