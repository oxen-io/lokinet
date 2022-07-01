#!/bin/bash
set -e
set +x

test x$NDK = x && echo "NDK env var not set"
test x$NDK = x && exit 1
root="$(readlink -f $(dirname $0)/../)"
cd "$root"
./contrib/android-configure.sh $@
make -C build-android -j ${JOBS:-$(nproc)}
