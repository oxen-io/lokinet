#!/bin/bash

set -e
set -x

if ! [ -f LICENSE ] || ! [ -d llarp ]; then
    echo "You need to run this as ./contrib/mac.sh from the top-level lokinet project directory" >&2
    exit 1
fi

mkdir -p build-mac
cd build-mac
cmake \
      -G Ninja \
      -DBUILD_STATIC_DEPS=ON \
      -DWITH_TESTS=OFF \
      -DWITH_BOOTSTRAP=OFF \
      -DNATIVE_BUILD=OFF \
      -DWITH_LTO=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DMACOS_SYSTEM_EXTENSION=ON \
      -DCODESIGN=ON \
      -DBUILD_PACKAGE=ON \
      "$@" \
      ..

echo "cmake build configured in build-mac"
