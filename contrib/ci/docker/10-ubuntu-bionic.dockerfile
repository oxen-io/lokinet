ARG ARCH=amd64
FROM ${ARCH}/ubuntu:bionic
RUN /bin/bash -c 'echo "man-db man-db/auto-update boolean false" | debconf-set-selections'
RUN apt-get -o=Dpkg::Use-Pty=0 -q update \
    && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y \
    && apt-get -o=Dpkg::Use-Pty=0 -q --no-install-recommends install -y \
        autoconf \
        automake \
        ccache \
        cmake \
        eatmydata \
        file \
        g++-8 \
        gdb \
        git \
        gperf \
        libjemalloc-dev \
        libpgm-dev \
        libtool \
        libuv1-dev \
        libzmq3-dev \
        lsb-release \
        make \
        ninja-build \
        openssh-client \
        openssh-client \
        patch \
        pkg-config \
        pybind11-dev \
        python3-dev \
        python3-pip \
        python3-pybind11 \
        python3-pytest \
        python3-setuptools \
        qttools5-dev \
    && mkdir -p /usr/lib/x86_64-linux-gnu/pgm-5.2/include
