ARG ARCH=amd64
FROM registry.oxen.rocks/lokinet-ci-debian-sid-base/${ARCH}
RUN apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y \
    clang-format-11 \
    eatmydata \
    git \
    jsonnet
