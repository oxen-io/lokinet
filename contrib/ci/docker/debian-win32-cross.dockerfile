ARG ARCH=amd64
FROM registry.oxen.rocks/lokinet-ci-debian-testing-base/${ARCH}
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y eatmydata build-essential cmake git ninja-build pkg-config ccache g++-mingw-w64-x86-64-posix nsis zip automake libtool autoconf make qttools5-dev file gperf patch openssh-client'
RUN /bin/bash -c 'update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix && update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix'
