#!/bin/bash
#
# configure step for ios

set -x

root=$(readlink -f "$( dirname $0 )/../../")

unset SDKROOT
export SDKROOT="$(xcrun --sdk iphoneos --show-sdk-path)"

targ=$1
plat=$2
shift
shift 

mkdir -p $targ

cmake \
    -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$root/external/ios-cmake/ios.toolchain.cmake" -DPLATFORM=$plat -DDEPLOYMENT_TARGET=13 -DENABLE_VISIBILITY=ON -DENABLE_BITCODE=OFF \
    -DAPPLE_IOS=ON \
    -DBUILD_STATIC_DEPS=ON \
    -DBUILD_PACKAGE=OFF \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DWITH_EMBEDDED_LOKINET=ON \
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
    -DCMAKE_CXX_FLAGS='-Oz' -DCMAKE_C_FLAGS='-Oz' \
    -DCMAKE_BUILD_TYPE=Release \
    -S "$root" -B $targ \
    $@
