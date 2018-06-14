This is a backport of the current C++ standard library to C++03/11/14. Obviously not everything
can be backported, but quite a bit can.

Quick Start
===========

There are a couple of small examples to give you an idea of usage. If you have make and g++
you can build things quickly enough:
 - clone the repo to /some/place/cppbackport
 - make a temp dir /wherever/you/wanna/build
 - cd /wherever/you/wanna/build
 - make -f /some/place/cppbackport/examples/Makefile

The examples are all setup to compile the cppbackport files as a static library (libcppbackport.a),
and then link it with one of the main example files (ex., gibberish.cpp).

Installation
============

I recommend copying the lib directory into your project, and calling the included Makefile
to create libcppbackport.a. Link that into your project, and add the appropriate include
flags (ex., -iquotecppbackport).

Usage
=====

Basically, #include "filesystem.h" or similar. Then use cpp namespace in place of std namespace.
The headers are all named after their official counterparts, with the addition of the .h
extension.

FAQ
===
**Who is this library suitable for?**  
Anyone, really. There are a few use cases:
- The primary use case is to provide some newer features to people who are stuck with/choose to use an older compiler
- Another use would be to soften the requirements of your own project (i.e., so your users can use an older compiler)
- In some cases, this project may provide usable code before compilers support a standard. For example, C++17 at the moment isn't even standardized, *but* we basically know what's in it so we can start supporting it.

**What's the license?**  
BSD 3-clause. Use it. Contribute if you like. Don't blame us for things.

**What compiler(s)/platforms are supported**  
The development environment is Fedora 24 with GCC 6.1.1. I've used it with earlier versions
of GCC (4.7.4, I think), and a semi-recent version of Clang. I've not tested under Windows, yet.

**Will this use C++11/14/17 if available?**  
Yes. Based on the value of the __cplusplus define, the files will simply #include the
system header (as appropriate).

**Why not header-only?**  
I actually like the interface/implementation distinction. Header-only implementations (IMO)
get way too large and the files are difficult to navigate. Also, save the compiler some work.
Also, installing a lib isn't really that hard.

**Does it work on Windows?**  
Sorry, POSIX mostly. Would love for some Windows devs to help.

**Why not use Boost?**  
There's some overlap, but there are differences, too. And none of these things, *individually*
are that big. 
