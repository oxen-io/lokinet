#!/bin/bash
set -e
set -x

root=$(readlink -f "$1")
shift
mkdir -p "$1"
build=$(readlink -f "$1")
shift
cd "$build"
cmake \
    -S "$root" -B "$build" \
    -G 'Unix Makefiles' \
    -DCMAKE_EXE_LINKER_FLAGS=-fstack-protector \
    -DCMAKE_CXX_FLAGS=-fdiagnostics-color=always \
    -DCMAKE_TOOLCHAIN_FILE="$root/contrib/cross/mingw64.cmake" \
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
    -DFORCE_OXENC_SUBMODULE=ON \
    -DFORCE_FMT_SUBMODULE=ON \
    -DFORCE_SPDLOG_SUBMODULE=ON \
    -DFORCE_NLOHMANN_SUBMODULE=ON \
    -DWITH_LTO=OFF \
    $@
