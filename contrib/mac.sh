#!/bin/bash
#
# Build the shit on mac
#
# You will generally need to add: -DCODESIGN_APP=... to make this work, and (unless you are a
# lokinet team member) will need to pay Apple money for your own team ID and arse around with
# provisioning profiles.  See macos/README.txt.
#

set -e
set -x

if ! [ -f LICENSE ] || ! [ -d llarp ]; then
    echo "You need to run this as ./contrib/mac.sh from the top-level lokinet project directory" >&2
    exit 1
fi

./contrib/mac-configure.sh "$@"

cd build-mac
rm -rf Lokinet\ *
ninja -j${JOBS:-1} dmg
cd ..

echo -e "Build complete, your app is here:\n"
ls -lad $(pwd)/build-mac/Lokinet\ *
echo ""
