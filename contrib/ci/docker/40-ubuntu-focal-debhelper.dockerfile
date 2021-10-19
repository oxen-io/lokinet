ARG ARCH=amd64
FROM registry.oxen.rocks/lokinet-ci-ubuntu-focal/${ARCH}
RUN apt-get -o=Dpkg::Use-Pty=0 --no-install-recommends -q install -y \
        ccache \
        debhelper \
        devscripts \
        equivs \
        git \
        git-buildpackage \
        python3-dev
