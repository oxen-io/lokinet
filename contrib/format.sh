#!/usr/bin/env bash

# TODO: readlink -e is a GNU-ism
cd "$(readlink -e $(dirname $0)/../)"
clang-format-11 -i $(find jni daemon llarp include pybind | grep -E '\.[hc](pp)?$') &> /dev/null
