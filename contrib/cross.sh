#!/bin/bash
#
# helper script for me for when i cross compile
# t. jeff
#
set -e

die() {
    echo $@
    exit 1
}

platform=${PLATFORM:-Linux}
root="$(readlink -e $(dirname $0)/../)"
cd $root
mkdir -p build-cross

targets=()
cmake_extra=()

while [ "$#" -gt 0 ]; do
    if [ "$1" = "--" ]; then
        shift
        cmake_extra=("$@")
        break
    fi
    targets+=("$1")
    shift
done
test ${#targets[@]} = 0 && die no targets provided

archs="${targets[@]}"
echo "all: $archs" > build-cross/Makefile
for arch in $archs ; do
    mkdir -p $root/build-cross/build-$arch
    cd $root/build-cross/build-$arch
    cmake \
        -G 'Unix Makefiles' \
        -DCROSS_PLATFORM=$platform \
        -DCROSS_PREFIX=$arch \
        -DCMAKE_EXE_LINKER_FLAGS=-fstack-protector \
        -DCMAKE_CXX_FLAGS=-fdiagnostics-color=always \
        -DCMAKE_TOOLCHAIN_FILE=$root/contrib/cross/cross.toolchain.cmake \
        -DBUILD_STATIC_DEPS=ON \
        -DSTATIC_LINK=ON \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_TESTING=OFF \
        -DWITH_EMBEDDED_LOKINET=OFF \
        -DWITH_TESTS=OFF \
        -DNATIVE_BUILD=OFF \
        -DSTATIC_LINK=ON \
        -DWITH_SYSTEMD=OFF \
        -DFORCE_OXENMQ_SUBMODULE=ON \
        -DSUBMODULE_CHECK=OFF \
        -DWITH_LTO=OFF \
        -DWITH_BOOTSTRAP=OFF \
        -DCMAKE_BUILD_TYPE=RelWithDeb \
        "${cmake_extra[@]}" \
        $root
    cd $root/build-cross
    echo -ne "$arch:\n\t\$(MAKE) -C  build-$arch\n" >> $root/build-cross/Makefile

done
cd $root
make -j${JOBS:-$(nproc)} -C build-cross
