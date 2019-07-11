
The windows release is signed by rick, his public key is:


```

-----BEGIN PGP PUBLIC KEY BLOCK-----

mDMEXBhAthYJKwYBBAHaRw8BAQdACj8fXcXB+ktPL/gNRBGZajE9ycsQOiMPXigH
0uP6BCW0G1JpY2sgViA8cmlja0Bzbm93bGlnaHQubmV0PoiQBBMWCAA4FiEEsbLw
yIc/Y1IT8TNLwO3Icj/cNGUFAlwYQLYCGyMFCwkIBwIGFQoJCAsCBBYCAwECHgEC
F4AACgkQwO3Icj/cNGUeCwEAuyFfehigul3So0xOuRIxldiHoqLJfSEp4kjU+8b5
NjsBAIOC4KFpdv8CTPa/aQgRIx/UlOjJ8vMnS94XPSs2vRcDuDgEXBhAthIKKwYB
BAGXVQEFAQEHQKT2GHP2O+q5vgXd6D4IiOu8rI+kcGllVY/0DEqGesJYAwEIB4h4
BBgWCAAgFiEEsbLwyIc/Y1IT8TNLwO3Icj/cNGUFAlwYQLYCGwwACgkQwO3Icj/c
NGX/tgD9GES37acIhovhMzDj0u9oU/1HqNyx4A45EQ90dP8KMN4BALBRzWXgB23t
9r6g3ZWHQJEpF4RnmcbDbR0SxdyoCkQG
=RdBx
-----END PGP PUBLIC KEY BLOCK-----

```

The linux and macos releases are signed by jeff, his public key is:

```

-----BEGIN PGP PUBLIC KEY BLOCK-----

mDMEWZx2ERYJKwYBBAHaRw8BAQdAKxsq4dGzYzKJqU8Vin5d8vJF10/NG4Hziw+f
WTbM8nC0MEplZmYgQmVja2VyIChwcm9iYWJseSBub3QgZXZpbCkgPGplZmZAaTJw
LnJvY2tzPoh5BBMWCAAhBQJZnHYRAhsDBQsJCAcCBhUICQoLAgQWAgMBAh4BAheA
AAoJEPNXs7Qvb5sFP2MBAIcL8KOd/RupEtSMyb2f4OBsaE8oFU+NsvfevW0XrBBQ
AQDhjax9f2D0k30pj4uYBJRb/L0JJFfbzI+uwgTtgRp1DLg4BFmcdhESCisGAQQB
l1UBBQEBB0BJOuegxPmX1Ma/nv4O2lZp0rA89EazPgtUrR3e1846DQMBCAeIYQQY
FggACQUCWZx2EQIbDAAKCRDzV7O0L2+bBUgkAPsEeiiut+gGECP/63m7NyTwruNP
oVZUYE1m8XXbHr28UgEA4nXGIAHDRuIUY4sRcVQz2Um9O6kaCdQHH0eSPE48VQ8=
=gFkp
-----END PGP PUBLIC KEY BLOCK-----

```

To verify the releases first import those keys into gpg key database

run the following, copy paste both keys and press `^D` (control - D) to finish

    $ gpg --import
    
Alternatively you can get jeff's key off a key server:

    $ gpg --recv-key 67EF6BA68E7B0B0D6EB4F7D4F357B3B42F6F9B05 # jeff's key
    
then verify the signatures, make sure that the `.sig` file and the release file
are in the same directory.

    $ gpg --verify release-file.exe.sig
