#!/bin/bash
#
# Build the shit on mac
#
# You will generally need to add: -DCODESIGN_APP=... to make this work, and (unless you are a
# lokinet team member) will need to pay Apple money for your own team ID and arse around with
# provisioning profiles.  See macos/README.txt.
#

set -e
set +x
if ! [ -f LICENSE ] || ! [ -d llarp ]; then
    echo "You need to run this as ./contrib/mac.sh from the top-level lokinet project directory"
fi

mkdir -p build-mac
cd build-mac
cmake \
      -G Ninja \
      -DBUILD_STATIC_DEPS=ON \
      -DBUILD_LIBLOKINET=OFF \
      -DWITH_TESTS=OFF \
      -DWITH_BOOTSTRAP=OFF \
      -DNATIVE_BUILD=OFF \
      -DWITH_LTO=ON \
      -DCMAKE_BUILD_TYPE=Release \
      "$@" \
      ..
ninja -j1

echo -e "Build complete, your app is here:\n"
ls -lad $(pwd)/Lokinet.app
echo ""
