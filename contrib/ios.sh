#!/bin/bash
#
# Build the shit for iphone, only builds embeded lokinet

set -e
set -x
if ! [ -f LICENSE ] || ! [ -d llarp ]; then
    echo "You need to run this as ./contrib/ios.sh from the top-level lokinet project directory"
fi

unset SDKROOT
export SDKROOT="$(xcrun --sdk iphoneos --show-sdk-path)"

mkdir -p build/iphone/{sim,device}

cmake \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=./contrib/cross/ios.toolchain.cmake -DPLATFORM=OS -DDEPLOYMENT_TARGET=13 -DENABLE_VISIBILITY=ON -DENABLE_BITCODE=OFF \
    -DCMAKE_CXX_COMPILER_LAUNCHER= -DCMAKE_C_COMPILER_LAUNCHER= \
    -DBUILD_STATIC_DEPS=ON \
    -DBUILD_PACKAGE=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_EMBEDDED_LOKINET=ON \
    -DWITH_TESTS=OFF \
    -DNATIVE_BUILD=OFF \
    -DSTATIC_LINK=ON \
    -DWITH_SYSTEMD=OFF \
    -DWITH_BOOTSTRAP=OFF \
    -DBUILD_DAEMON=OFF \
    -DFORCE_OXENMQ_SUBMODULE=ON \
    -DFORCE_OXENC_SUBMODULE=ON \
    -DFORCE_NLOHMANN_SUBMODULE=ON \
    -DFORCE_LIBUV_SUBMODULE=ON \
    -DSUBMODULE_CHECK=ON \
    -DWITH_LTO=OFF \
    -S . -B build/iphone/device \
    $@

cmake \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=./contrib/cross/ios.toolchain.cmake -DPLATFORM=SIMULATOR64 -DDEPLOYMENT_TARGET=13 -DENABLE_VISIBILITY=ON -DENABLE_BITCODE=OFF \
    -DCMAKE_CXX_COMPILER_LAUNCHER= -DCMAKE_C_COMPILER_LAUNCHER= \
    -DBUILD_STATIC_DEPS=ON \
    -DBUILD_PACKAGE=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_EMBEDDED_LOKINET=ON \
    -DWITH_TESTS=OFF \
    -DNATIVE_BUILD=OFF \
    -DSTATIC_LINK=ON \
    -DWITH_SYSTEMD=OFF \
    -DWITH_BOOTSTRAP=OFF \
    -DBUILD_DAEMON=OFF \
    -DFORCE_OXENMQ_SUBMODULE=ON \
    -DFORCE_OXENC_SUBMODULE=ON \
    -DFORCE_NLOHMANN_SUBMODULE=ON \
    -DFORCE_LIBUV_SUBMODULE=ON \
    -DSUBMODULE_CHECK=ON \
    -DWITH_LTO=OFF \
    -S . -B build/iphone/sim \
    $@

for targ in device sim ; do
    cmake --build build/iphone/$targ --target lokinet-embedded
done
