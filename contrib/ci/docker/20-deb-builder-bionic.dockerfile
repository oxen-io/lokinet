ARG ARCH=amd64
FROM registry.oxen.rocks/lokinet-ci-ubuntu-bionic-base/${ARCH}
RUN apt-get -o=Dpkg::Use-Pty=0 --no-install-recommends -q install -y \
        ccache \
        cmake \
        devscripts \
        equivs \
        g++ \
        git-buildpackage \
        openssh-client
