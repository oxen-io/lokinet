#!/bin/bash
set -e
set +x

root="$(readlink -f $(dirname $0)/../)"
cd "$root"
./contrib/android-configure.sh . build-android "$@"
make -C build-android -j ${JOBS:-$(nproc)}
