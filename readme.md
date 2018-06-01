# LLARP

Low Latency Anon Routing Protocol

We'll be ready when we're ready.

[who](https://github.com/majestrate)

[what + why](doc/high-level.txt)

[when](http://i0.kym-cdn.com/photos/images/original/000/117/021/enhanced-buzz-28895-1301694293-0.jpg)

[where](https://joedaeskimo.files.wordpress.com/2011/01/idklol.jpg)

[how](doc/proto_v0.txt)


## Building

You have 2 ways the build this project

### The recommended way (for stable builds)

    $ git clone --recursive https://github.com/majestrate/llarpd-builder
    $ cd llarpd-builder
    $ make 

### The "I want to risk ripping my fingernails off in frustration" way (for dev builds)

Build requirements:

* CMake
* ninja
* libsodium >= 1.0.14 
* c++ 17 capable C++ compiler
* c11 capable C compiler


Building a debug build:

    $ make
    

## Running

Right now the reference daemon connects to nodes you tell it to and that's it.

If you built using the `recommended` way just run:

    $ ./llarpd
    
It'll attempt to connect to a test node I run and keep the session alive.
That's it.

If you built using the dev build you are expected to configure the daemon yourself.
