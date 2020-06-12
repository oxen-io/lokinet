#!/usr/bin/env bash
cd "$(readlink -e $(dirname $0)/../)"
clang-format-9 -i $(find jni daemon llarp include pybind | grep -E '\.[hc](pp)?$') &> /dev/null
