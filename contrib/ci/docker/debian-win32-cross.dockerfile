ARG ARCH=amd64
FROM registry.oxen.rocks/lokinet-ci-debian-testing-base/${ARCH}
RUN apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y \
        autoconf \
        automake \
        build-essential \
        ccache \
        cmake \
        eatmydata \
        file \
        g++-mingw-w64-x86-64-posix \
        git \
        gperf \
        libtool \
        make \
        ninja-build \
        nsis \
        openssh-client \
        patch \
        pkg-config \
        qttools5-dev \
        zip \
    && update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix \
    && update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
