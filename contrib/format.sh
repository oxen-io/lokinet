#!/usr/bin/env bash

CLANG_FORMAT_DESIRED_VERSION=11

binary=$(command -v clang-format-$CLANG_FORMAT_DESIRED_VERSION 2>/dev/null)
if [ $? -ne 0 ]; then
    binary=$(command -v clang-format-mp-$CLANG_FORMAT_DESIRED_VERSION 2>/dev/null)
fi
if [ $? -ne 0 ]; then
    binary=$(command -v clang-format 2>/dev/null)
    if [ $? -ne 0 ]; then
        echo "Please install clang-format version $CLANG_FORMAT_DESIRED_VERSION and re-run this script."
        exit 1
    fi
    version=$(clang-format --version)
    if [[ ! $version == *"clang-format version $CLANG_FORMAT_DESIRED_VERSION"* ]]; then
        echo "Please install clang-format version $CLANG_FORMAT_DESIRED_VERSION and re-run this script."
        exit 1
    fi
fi

cd "$(dirname $0)/../"
if [ "$1" = "verify" ] ; then
    if [ $($binary --output-replacements-xml $(find jni daemon llarp include pybind | grep -E '\.([hc](pp)?|m(m)?)$' | grep -v '\#') | grep '</replacement>' | wc -l) -ne 0 ] ; then
        exit 2
    fi
else
    $binary -i $(find jni daemon llarp include pybind | grep -E '\.([hc](pp)?|m(m)?)$' | grep -v '\#') &> /dev/null
fi

swift_format=$(command -v swiftformat 2>/dev/null)
if [ $? -eq 0 ]; then
    if [ "$1" = "verify" ] ; then
        for f in $(find daemon | grep -E '\.swift$' | grep -v '\#') ; do
            if [ $($swift_format --quiet --dryrun < "$f" | diff "$f" - | wc -l) -ne 0 ] ; then
                exit 3
            fi
        done
    else
        $swift_format --quiet $(find daemon | grep -E '\.swift$' | grep -v '\#')
    fi

fi

jsonnet_format=$(command -v jsonnetfmt 2>/dev/null)
if [ $? -eq 0 ]; then
    if [ "$1" = "verify" ]; then
        if ! $jsonnet_format --test .drone.jsonnet; then
            exit 4
        fi
    else
        $jsonnet_format --in-place .drone.jsonnet
    fi
fi
