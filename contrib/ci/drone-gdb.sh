#!/usr/bin/env bash
rm -f crash.out.txt exit.out.txt
gdb -q -x $(readlink -e $(dirname $0))/gdb-filter.py --args $@
test -e crash.out.txt && cat crash.out.txt
exit $(cat exit.out.txt)
