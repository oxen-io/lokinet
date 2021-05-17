#!/bin/bash
#
# pre-push hook for git
# this script is probably overkill for most contributors
#
# "i use this to prevent foot cannons caused by commiting broken code"
#
# ~ jeff (lokinet author and crazy person)
#
#
# to use this as a git hook do this in the root of the repo:
#
# cp contrib/git-hook-pre-push.sh .git/hooks/pre-push
#


set -e

cd "$(dirname $0)/../.."
echo "check format..."
./contrib/format.sh verify
echo "format is gucci af fam"

echo "remove old test build directory..."
rm -rf build-git-hook
mkdir build-git-hook
echo "configuring test build jizz..."
cmake -S . -B build-git-hook -DWITH_LTO=OFF -DWITH_HIVE=ON -G Ninja
echo "ensure this shit compiles..."
ninja -C build-git-hook all
echo "ensure unit tests aren't fucked..."
ninja -C build-git-hook check

echo "we gud UmU"
echo ""
