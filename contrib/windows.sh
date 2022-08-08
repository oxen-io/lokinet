#!/bin/bash
#
# helper script for me for when i cross compile for windows
# t. jeff
#

set -e
set +x


root="$(readlink -f $(dirname $0)/../)"
cd "$root"
./contrib/windows-configure.sh . build-windows "$@"
make package -j${JOBS:-$(nproc)} -C build-windows
