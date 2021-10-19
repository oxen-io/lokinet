FROM node:14.16.1
RUN /bin/bash -c 'echo "man-db man-db/auto-update boolean false" | debconf-set-selections'
RUN apt-get -o=Dpkg::Use-Pty=0 -q update \
    && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y \
    && apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y \
        ccache \
        cmake \
        eatmydata \
        g++ \
        gdb \
        git \
        make \
        ninja-build \
        openssh-client \
        patch \
        pkg-config \
        wine
