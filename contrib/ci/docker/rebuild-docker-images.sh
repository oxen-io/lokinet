#!/bin/bash

set -o errexit

trap 'echo -e "\n\n\n\e[31;1mAn error occurred!\e[1m\n\n"' ERR

registry=registry.oxen.rocks

if [[ $# -eq 0 ]]; then
    files=(*.dockerfile i386/*.dockerfile arm64v8/*.dockerfile arm32v7/*.dockerfile)
else
    files=("$@")
fi

declare -A manifests

for file in "${files[@]}"; do
    if [[ "$file" == */* ]]; then
        arch="${file%%/*}"
        name="${file#*/}"
    else
        arch="amd64"
        name="$file"
    fi

    name="${name#[0-9][0-9]-}" # s/^\d\d-//
    name="${name%.dockerfile}" # s/\.dockerfile$//
    namearch=$registry/lokinet-ci-$name/$arch
    latest=$registry/lokinet-ci-$name:latest
    echo -e "\e[32;1mrebuilding \e[35;1m$namearch\e[0m"
    docker build --pull -f $file -t $namearch --build-arg ARCH=$arch $DOCKER_BUILD_OPTS .
    docker push $namearch

    manifests[$latest]="${manifests[$latest]} $namearch"
done

for latest in "${!manifests[@]}"; do
    echo -e "\e[32;1mpushing new manifest for \e[33;1m$latest[\e[35;1m${manifests[$latest]} \e[33;1m]\e[0m"
    docker manifest rm $latest 2>/dev/null || true
    docker manifest create $latest ${manifests[$latest]}
    docker manifest push $latest
done

echo -e "\n\n\n\e[32;1mAll done!\e[1m\n\n"
