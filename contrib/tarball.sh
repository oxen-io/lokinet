#!/usr/bin/env bash
#
# create signed release tarball with submodules bundled
#
repo=$(readlink -e $(dirname $0)/..)
branch=$(test -e $repo/.git/ && git rev-parse --abbrev-ref HEAD)
out="lokinet-$(git describe --exact-match --tags $(git log -n1 --pretty='%h') 2> /dev/null || ( echo -n $branch- && git rev-parse --short HEAD)).tar.xz"
git-archive-all -C $repo --force-submodules $out && rm -f $out.sig && (gpg -u jeff@lokinet.io --sign --detach $out &> /dev/null && gpg --verify $out.sig)
