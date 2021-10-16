#!/bin/bash

set -o errexit

registry=registry.oxen.rocks

if [[ $# -eq 0 ]]; then
    files=(*.dockerfile)
else
    files=("$@")
fi

for file in "${files[@]}"; do
    name="${file#[0-9][0-9]-}" # s/^\d\d-//
    name="${name%.dockerfile}" # s/\.dockerfile$//
    echo -e "\e[32;1mrebuilding $name\e[0m"
    docker build --pull -f $file -t $registry/lokinet-ci-$name .
    docker push $registry/lokinet-ci-$name
done
