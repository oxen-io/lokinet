#!/bin/bash
#
# Build the shit for iphone, only builds embeded lokinet

set -e
set -x
if ! [ -f LICENSE ] || ! [ -d llarp ]; then
    echo "You need to run this as ./contrib/ios.sh from the top-level lokinet project directory"
fi

mkdir -p build/iphone
cd build/iphone
cmake \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=../../external/ios-cmake/ios.toolchain.cmake \
    -DBUILD_STATIC_DEPS=ON \
    -DBUILD_PACKAGE=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_LIBLOKINET=ON \
    -DWITH_TESTS=OFF \
    -DNATIVE_BUILD=OFF \
    -DSTATIC_LINK=ON \
    -DWITH_SYSTEMD=OFF \
    -DFORCE_OXENMQ_SUBMODULE=ON \
    -DFORCE_OXENC_SUBMODULE=ON \
    -DSUBMODULE_CHECK=ON \
    -DWITH_LTO=OFF \
    -DPLATFORM=OS64COMBINED \
    -DCMAKE_BUILD_TYPE=Release \
    "$@" \
    ../..
ninja lokinet-shared
