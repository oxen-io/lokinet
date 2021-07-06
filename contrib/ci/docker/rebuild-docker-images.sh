#!/bin/bash

# the registry server to use
registry=$1

test "x$registry" != "x" || exit 1

for file in ${@:2} ; do
    name="$(echo $file | cut -d'.' -f1)"
    echo "rebuild $name"
    docker build -f $file -t $registry/lokinet-ci-$name .
    docker push $registry/lokinet-ci-$name
done
