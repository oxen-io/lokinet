FROM debian:testing
RUN /bin/bash -c 'echo "man-db man-db/auto-update boolean false" | debconf-set-selections'
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q update && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y && apt-get -o=Dpkg::Use-Pty=0 -q install -y eatmydata build-essential cmake git ninja-build pkg-config ccache g++-mingw-w64-x86-64-posix nsis zip automake libtool autoconf make qttools5-dev file gperf patch openssh-client'
RUN /bin/bash -c 'update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix && update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix'
