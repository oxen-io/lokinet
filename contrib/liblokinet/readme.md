# liblokinet examples

building:

    $ mkdir -p build
    $ cd build
    $ cp /path/to/liblokinet.so .
    $ cmake .. -DCMAKE_EXE_LINKER_FLAGS='-L.'
    $ make

running:

    $ ./udptest /path/to/bootstrap.signed
