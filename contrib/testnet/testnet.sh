#!/usr/bin/env bash

for arg in "$1" "$2" "$3" ; do
    test x = "x$arg" && echo "usage: $0 path/to/lokinet num_svc num_clients" && exit 1;
done

script_root=$(dirname $(readlink -e $0))
testnet_dir=/tmp/lokinet-testnet

mkdir -p $testnet_dir

set -x

$script_root/genconf.py --bin $1 --netid=testnet --out=$testnet_dir/testnet.ini --svc $2 --dir=$testnet_dir --clients $3 || exit 1
supervisord -n -c $testnet_dir/testnet.ini
