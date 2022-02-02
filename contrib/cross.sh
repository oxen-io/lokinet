#!/bin/bash
#
# helper script for me for when i cross compile
# t. jeff
#
die() {
    echo $@
    exit 1
}

platform=${PLATFORM:-Linux}
root="$(readlink -e $(dirname $0)/../)"
cd $root
set -e
set +x
test $# = 0 && die no targets provided
mkdir -p build-cross
echo "all: $@" > build-cross/Makefile
for targ in $@ ; do
    mkdir -p $root/build-cross/build-$targ
    cd $root/build-cross/build-$targ
    cmake \
        -G 'Unix Makefiles' \
        -DCROSS_PLATFORM=$platform \
        -DCROSS_PREFIX=$targ \
        -DCMAKE_EXE_LINKER_FLAGS=-fstack-protector \
        -DCMAKE_CXX_FLAGS=-fdiagnostics-color=always\
        -DCMAKE_TOOLCHAIN_FILE=$root/contrib/cross/cross.toolchain.cmake\
        -DBUILD_STATIC_DEPS=ON \
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
        -DWITH_BOOTSTRAP=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        $root
    cd $root/build-cross
    echo -ne "$targ:\n\t\$(MAKE) -C  build-$targ\n" >> $root/build-cross/Makefile

done
cd $root
make -j${JOBS:-$(nproc)} -C build-cross
