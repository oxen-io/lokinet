FROM registry.oxen.rocks/lokinet-ci-debian-sid-base
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y eatmydata git clang-format-11 jsonnet'
