#!/usr/bin/env bash

CLANG_FORMAT_DESIRED_VERSION=11

binary=$(which clang-format-$CLANG_FORMAT_DESIRED_VERSION 2>/dev/null)
if [ $? -ne 0 ]; then
    binary=$(which clang-format 2>/dev/null)
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
    exit $($binary --output-replacements-xml $(find jni daemon llarp include pybind | grep -E '\.[hc](pp)?$' | grep -v '\#') | grep '</replacement>' | wc -l)
else
    $binary -i $(find jni daemon llarp include pybind | grep -E '\.[hc](pp)?$' | grep -v '\#') &> /dev/null
fi
