#!/bin/bash

# Script used with Drone CI to check that a statically build lokinet only links against the expected
# base system libraries.  Expects to be run with pwd of the build directory.

set -o errexit

if ldd daemon/lokinet | grep -Ev '(linux-vdso|ld-linux-x86-64|lib(pthread|dl|rt|stdc\+\+|gcc_s|c|m))\.so'; then
    echo -e "\n\n\n\n\e[31;1mlokinet links to unexpected libraries\e[0m\n\n\n"
    exit 1
fi

echo -e "\n\n\n\n\e[32;1mNo unexpected linked libraries found\e[0m\n\n\n"
