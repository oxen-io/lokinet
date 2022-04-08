#!/bin/bash
#
# helper script for me for when i cross compile for windows
# t. jeff
#

set -e
set -x
root="$(readlink -f $(dirname $0)/../)"
mkdir -p build-windows
cmake \
    -S "$root" \
    -B build-windows \
    -G 'Unix Makefiles' \
    -DCMAKE_EXE_LINKER_FLAGS=-fstack-protector \
    -DCMAKE_CXX_FLAGS=-fdiagnostics-color=always\
    -DCMAKE_TOOLCHAIN_FILE=$root/contrib/cross/mingw64.cmake\
    -DBUILD_STATIC_DEPS=ON \
    -DBUILD_PACKAGE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_LIBLOKINET=ON \
    -DWITH_TESTS=OFF \
    -DNATIVE_BUILD=OFF \
    -DSTATIC_LINK=ON \
    -DWITH_SYSTEMD=OFF \
    -DFORCE_OXENMQ_SUBMODULE=ON \
    -DSUBMODULE_CHECK=OFF \
    -DWITH_LTO=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    $@
