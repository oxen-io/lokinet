#!/bin/bash
#
# helper script for me for when i cross compile
# t. jeff
#
die() {
    echo $@
    exit 1
}

root="$(readlink -e $(dirname $0)/../)"
cd $root
set -e
set +x
test $# = 0 && die no targets provided
mkdir -p build-cross
echo "all: $@" > build-cross/Makefile
for targ in $@ ; do
    mkdir -p build-$targ
    cd build-$targ
    cmake \
        -G 'Unix Makefiles' \
        -DCMAKE_EXE_LINKER_FLAGS=-fstack-protector \
        -DCMAKE_CXX_FLAGS=-fdiagnostics-color=always\
        -DCMAKE_TOOLCHAIN_FILE=../contrib/cross/$targ.toolchain.cmake\
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
        ..
    cd -
    echo -ne "$targ:\n\t\$(MAKE) -C $root/build-$targ\n" >> build-cross/Makefile

done

make -j${JOBS:-$(nproc)} -C build-cross
