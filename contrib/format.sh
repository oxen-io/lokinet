#!/usr/bin/env bash

. $(dirname $0)/format-version.sh

cd "$(dirname $0)/../"

if [ "$1" = "verify" ] ; then
    if [ $($CLANG_FORMAT --output-replacements-xml $(find jni daemon llarp include pybind | grep -E '\.([hc](pp)?|m(m)?)$' | grep -v '#') | grep '</replacement>' | wc -l) -ne 0 ] ; then
        exit 2
    fi
else
    $CLANG_FORMAT -i $(find jni daemon llarp include pybind | grep -E '\.([hc](pp)?|m(m)?)$' | grep -v '#') &> /dev/null
fi

swift_format=$(command -v swiftformat 2>/dev/null)
if [ $? -eq 0 ]; then
    if [ "$1" = "verify" ] ; then
        for f in $(find daemon | grep -E '\.swift$' | grep -v '#') ; do
            if [ $($swift_format --quiet --dryrun < "$f" | diff "$f" - | wc -l) -ne 0 ] ; then
                exit 3
            fi
        done
    else
        $swift_format --quiet $(find daemon | grep -E '\.swift$' | grep -v '#')
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
