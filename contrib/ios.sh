#!/bin/bash
#
# Build the shit for iphone, only builds embeded lokinet

set -e
set -x
if ! [ -f LICENSE ] || ! [ -d llarp ]; then
    echo "You need to run this as ./contrib/ios.sh from the top-level lokinet project directory"
fi


root="$(readlink -f $(dirname $0)/../)"

build_dir="$root/build/iphone"

./contrib/ios/ios-configure.sh "$build_dir/device" OS64 $@
./contrib/ios/ios-configure.sh "$build_dir/sim" SIMULATOR64 $@

for targ in device sim ; do
    ./contrib/ios/ios-build.sh "$build_dir/$targ"
done

pkg_name="iphone_lokinet_embedded_$(date +%s)"
pkg_dir="$build_dir/$pkg_name"

mkdir -p "$pkg_dir"/{include,lib/{device,sim}}
for targ in device sim ; do 
    mkdir -p "$pkg_dir/lib/$targ"
    cp -av "$build_dir/$targ/llarp/liblokinet-embedded.a" "$pkg_dir/lib/$targ/liblokinet-embedded.a"
done
cp -a "$root"/include/lokinet{,/*}.h "$pkg_dir/include/"

cd "$build_dir"
tar cfv "$pkg_name.tar" $pkg_name
cd -
xz -T 0 "$build_dir/$pkg_name.tar"
