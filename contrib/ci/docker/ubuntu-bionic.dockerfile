FROM ubuntu:bionic
RUN /bin/bash -c 'echo "man-db man-db/auto-update boolean false" | debconf-set-selections'
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q update && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y && apt-get -o=Dpkg::Use-Pty=0 -q --no-install-recommends install -y eatmydata gdb cmake git ninja-build pkg-config ccache g++-8 python3-dev automake libtool autoconf make qttools5-dev file gperf patch openssh-client'
