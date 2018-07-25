#!/usr/bin/env bash
root=$(dirname $(realpath -L $0))

if [ ! -d v ] ; then 
  echo "setting up wizard for the first time..."
  python3 -m venv v && v/bin/pip install -r "$root/requirements.txt" &> /dev/null || echo "failed"
fi
v/bin/python "$root/lokinet.py" $@