#!/usr/bin/python3

import sys

base32z_dict = 'ybndrfg8ejkmcpqxot1uwisza345h769'
base32z_map = {base32z_dict[i]: i for i in range(len(base32z_dict))}

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


def hex_from_snode(b32z):
    """undoes what the above does; b32z should have '.snode' already stripped off"""
    assert(len(b32z) == 52)
    val = 0
    bits = 0
    for x in b32z:
        val = (val << 5) | base32z_map[x]  # Arbitrary precision integers FTW

    # `val` is now a 260 bit value (52 * 5 bits per char); but we only use the first bit of the last
    # value (which is why lokinet addresses always end with y or o)
    assert(b32z[-1] in 'yo')
    val >>= 4

    return "{:64x}".format(val)


reverse = False
if len(sys.argv) >= 2 and sys.argv[1] == '-r':
    reverse = True
    del sys.argv[1]

if len(sys.argv) < 2 or (
        any(len(x) not in (52, 58) for x in sys.argv[1:])
        if reverse else
        any(len(x) != 64 for x in sys.argv[1:])
        ):
    print("Usage: {} PUBKEY [PUBKEY ...] -- converts ed25519 pubkeys to .snode addresses".format(sys.argv[0]))
    print("Usage: {} -r SNODE [SNODE ...] -- converts snode addresses to ed25519 pubkeys".format(sys.argv[0]))
    sys.exit(1)

if reverse:
    for key in sys.argv[1:]:
        print("{}.snode -> {}".format(key[0:52], hex_from_snode(key[0:52])))
else:
    for key in sys.argv[1:]:
        print("{} -> {}".format(key, lokinet_snode_addr(key)))
