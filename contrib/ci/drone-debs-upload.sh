#!/bin/bash

# Script used with Drone CI to upload debs from the deb building pipelines (because specifying all
# this in .drone.jsonnet is too painful).  This is expected to run from the base project dir after
# having build with debuild (which will leave the debs in ..).

set -o errexit

distro="$1"

if [ -z "$distro" ]; then
    echo "Bad usage: need distro name as first argument"
    exit 1
fi

if [ -z "$SSH_KEY" ]; then
    echo -e "\n\n\n\e[31;1mUnable to upload debs: SSH_KEY not set\e[0m"
    # Just warn but don't fail, so that this doesn't trigger a build failure for untrusted builds
    exit 0
fi

echo "$SSH_KEY" >~/ssh_key

set -o xtrace  # Don't start tracing until *after* we write the ssh key

chmod 600 ~/ssh_key

upload_to="oxen.rocks/debs/${DRONE_REPO// /_}@${DRONE_BRANCH// /_}/$(date --date=@$DRONE_BUILD_CREATED +%Y%m%dT%H%M%SZ)-${DRONE_COMMIT:0:9}/$distro/$DRONE_STAGE_ARCH"

# sftp doesn't have any equivalent to mkdir -p, so we have to split the above up into a chain of
# -mkdir a/, -mkdir a/b/, -mkdir a/b/c/, ... commands.  The leading `-` allows the command to fail
# without error.
upload_dirs=(${upload_to//\// })
mkdirs=
dir_tmp=""
for p in "${upload_dirs[@]}"; do
    dir_tmp="$dir_tmp$p/"
    mkdirs="$mkdirs
-mkdir $dir_tmp"
done

sftp -i ~/ssh_key -b - -o StrictHostKeyChecking=off drone@oxen.rocks <<SFTP
$mkdirs
put ../*.*deb $upload_to
SFTP

set +o xtrace

echo -e "\n\n\n\n\e[32;1mUploaded debs to https://${upload_to}/\e[0m\n\n\n"

