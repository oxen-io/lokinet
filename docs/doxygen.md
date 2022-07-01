
# Doxygen

building doxygen docs requires the following:

* cmake
* doxygen
* sphinx-build
* sphinx readthedocs theme
* breathe
* exhale

install packages:

    $ sudo apt install make cmake doxygen python3-sphinx python3-sphinx-rtd-theme python3-breathe python3-pip
    $ pip3 install --user exhale
    
build docs:

    $ mkdir -p build-docs
    $ cd build-docs
    $ cmake .. && make doc 

serve built docs via http, will be served at http://127.0.0.1:8000/

    $ python3 -m http.server -d docs/html
    
