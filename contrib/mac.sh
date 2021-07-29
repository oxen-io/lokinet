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
if ! [ -f LICENSE.txt ] || ! [ -d llarp ]; then
    echo "You need to run this as ./contrib/mac.sh from the top-level lokinet project directory"
fi

mkdir -p build-mac
cd build-mac
cmake \
      -G Ninja \
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
      -DSUBMODULE_CHECK=OFF \
      -DWITH_LTO=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      "$@" \
      ..
ninja sign

echo -e "Build complete, your app is here:\n"
ls -lad daemon/lokinet.app
echo ""
