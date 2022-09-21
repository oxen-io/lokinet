#!/bin/bash
#
# helper script for me for when i cross compile for windows
# t. jeff
#

set -e
set +x
root="$(readlink -f $(dirname $0)/../)"
mkdir -p $root/build/win32
$root/contrib/windows-configure.sh $root $root/build/win32 "$@"
make package -j${JOBS:-$(nproc)} -C $root/build/win32

