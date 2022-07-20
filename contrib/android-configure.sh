#!/bin/bash
set -e

default_abis="armeabi-v7a arm64-v8a x86_64"
build_abis=${ABIS:-$default_abis}

test x$NDK = x && test -e /usr/lib/android-ndk && export NDK=/usr/lib/android-ndk
test x$NDK = x && exit 1

echo "building abis: $build_abis"

root=$(readlink -f "$1")
shift
build=$(readlink -f "$1")
shift
mkdir -p $build
cd $build

for abi in $build_abis; do
    mkdir -p build-$abi
    cd build-$abi
    cmake \
        -S "$root" -B . \
        -G 'Unix Makefiles' \
        -DANDROID=ON \
        -DANDROID_ABI=$abi \
        -DANDROID_ARM_MODE=arm \
        -DANDROID_PLATFORM=android-23 \
        -DANDROID_STL=c++_static \
        -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
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
        -DSUBMODULE_CHECK=OFF \
        -DWITH_LTO=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        $@
    cd -
done
rm -f $build/Makefile
echo "# generated makefile" >> $build/Makefile
echo "all: $build_abis" >> $build/Makefile
for abi in $build_abis; do
    echo -ne "$abi:\n\t" >> $build/Makefile
    echo -ne '$(MAKE) -C ' >> $build/Makefile
    echo "build-$abi lokinet-android" >> $build/Makefile
    echo -ne "\tmkdir -p out/$abi && cp build-$abi/jni/liblokinet-android.so out/$abi/liblokinet-android.so\n\n" >> $build/Makefile
    echo -ne "clean-$abi:\n\t" >> $build/Makefile
    echo -ne '$(MAKE) -C ' >> $build/Makefile
    echo "build-$abi clean" >> $build/Makefile
done

echo -ne "clean:" >> $build/Makefile
for targ in $build_abis ; do echo -ne " clean-$targ" >> $build/Makefile ; done
echo "" >> $build/Makefile
