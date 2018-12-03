# lokinet vanity address generator

installing deps:

    sudo apt install libsodium-dev
    pip3 install --user -r requirements.txt

to generate a nonce with a prefix `^7oki` using 8 cpu threads:

    python3 lokinet-vanity.py keyfile.private 7oki 8
