FROM registry.oxen.rocks/lokinet-ci-debian-sid
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y clang-13 lld-13 libc++-13-dev libc++abi-13-dev'
