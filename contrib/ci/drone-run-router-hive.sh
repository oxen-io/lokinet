#!/usr/bin/env bash
export PYTHONPATH=pybind
rm -f crash.out.txt exit.out.txt
gdb -q -x $(readlink -e $(dirname $0))/gdb-filter.py --args /usr/bin/python3 -m pytest ../test/
test -e crash.out.txt && cat crash.out.txt
exit $(cat exit.out.txt)
