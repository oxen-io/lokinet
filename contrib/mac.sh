#!/bin/bash
#
# build the shit on mac
# t. jeff
#

set -e
set +x
mkdir -p build-mac
cd build-mac
cmake \
      -G Ninja \
      -DBUILD_STATIC_DEPS=ON \
      -DBUILD_PACKAGE=ON \
      -DBUILD_SHARED_LIBS=OFF \
      -DBUILD_TESTING=OFF \
      -DBUILD_LIBLOKINET=OFF \
      -DWITH_TESTS=OFF \
      -DNATIVE_BUILD=OFF \
      -DSTATIC_LINK=ON \
      -DWITH_SYSTEMD=OFF \
      -DFORCE_OXENMQ_SUBMODULE=ON \
      -DSUBMODULE_CHECK=OFF \
      -DWITH_LTO=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      "$@" \
      ..
ninja install && ninja sign
