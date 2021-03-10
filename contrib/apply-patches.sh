#!/usr/bin/env bash
for f in "$@" ; do
    patch -p1 -i "$f"
done
