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

deviceArchs=(arm64)
devicePlat=(OS64)
simArchs=(arm64 x86_64)
simPlat=(SIMULATORARM64 SIMULATOR64)

for i in "${!deviceArchs[@]}"; do
    ./contrib/ios/ios-configure.sh "$build_dir/device/${deviceArchs[i]}" ${devicePlat[i]} $@
done

for i in "${!simArchs[@]}"; do
    ./contrib/ios/ios-configure.sh "$build_dir/sim/${simArchs[i]}" ${simPlat[i]} $@
done

for targ in ${deviceArchs[@]} ; do
    ./contrib/ios/ios-build.sh "$build_dir/device/$targ"
done

for targ in ${simArchs[@]} ; do
    ./contrib/ios/ios-build.sh "$build_dir/sim/$targ"
done

pkg_name="iphone_lokinet_embedded_$(date +%s)"
pkg_dir="$build_dir/$pkg_name"

# Combine and device/simulator libraries into a single file so XCode doesn't complain
function combineArchsIfNeeded() {
    local group=$1
    local destination=$2
    shift; shift
    local archs=("$@")

    if [ ${#archs[@]} -gt 1 ]; then
        local dirs=("${archs[@]/#/$build_dir/${group}/}")
        local libs=("${dirs[@]/%//llarp/liblokinet-embedded.a}")

        mkdir -p "$build_dir/$group/$destination/llarp"
        lipo -create "${libs[@]}" -output "$build_dir/$group/${destination}/llarp/liblokinet-embedded.a"
    fi
}

deviceArchsString="${deviceArchs[*]}"
joinedDeviceArchs="${deviceArchsString// /_}"
simArchsString="${simArchs[*]}"
joinedSimArchs="${simArchsString// /_}"

combineArchsIfNeeded "device" $joinedDeviceArchs ${deviceArchs[*]}
combineArchsIfNeeded "sim" $joinedSimArchs ${simArchs[*]}

# Create a '.xcframework' so XCode can deal with the different architectures
xcodebuild -create-xcframework \
    -library "$build_dir/device/$joinedDeviceArchs/llarp/liblokinet-embedded.a" \
    -library "$build_dir/sim/$joinedSimArchs/llarp/liblokinet-embedded.a" \
    -output "$pkg_dir/libLokinet.xcframework"

# Copy the headers over
mkdir -p "$pkg_dir/libLokinet.xcframework/lokinet"
cp -a "$root"/include/lokinet{,/*}.h "$pkg_dir/libLokinet.xcframework/lokinet"
mv "$pkg_dir/libLokinet.xcframework/lokinet/lokinet.h" "$pkg_dir/libLokinet.xcframework/lokinet.h"

# The 'module.modulemap' is needed for XCode to be able to find the headers
echo -e 'module Lokinet {' > "$pkg_dir/libLokinet.xcframework/module.modulemap"
echo -e '  umbrella header "lokinet.h"' >> "$pkg_dir/libLokinet.xcframework/module.modulemap"
echo -e '  export *' >> "$pkg_dir/libLokinet.xcframework/module.modulemap"
echo -e '}' >> "$pkg_dir/libLokinet.xcframework/module.modulemap"

# Archive the result
cd "$build_dir"
tar cfv "$pkg_name.tar" $pkg_name
cd -
xz -T 0 "$build_dir/$pkg_name.tar"
