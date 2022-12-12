#!/bin/bash
#
# Build the shit for iphone

test -e $1 || ( echo "run ios-configure.sh first" ; exit 1 )

cmake --build $1 --target lokinet-embedded
