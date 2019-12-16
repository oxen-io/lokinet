#!/usr/bin/env bash
echo -n 'l'
for arg in $@ ; do cat "$arg" ; done
echo -n 'e'
