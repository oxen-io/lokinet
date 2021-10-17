ARG ARCH=amd64
FROM registry.oxen.rocks/lokinet-ci-debian-bullseye-base/${ARCH}
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y eatmydata gdb cmake make patch git ninja-build pkg-config ccache g++ libsodium-dev libzmq3-dev libsystemd-dev python3-dev libuv1-dev libunbound-dev nettle-dev libssl-dev libevent-dev libsqlite3-dev libboost-thread-dev libboost-serialization-dev libboost-program-options-dev libgtest-dev libminiupnpc-dev libunwind8-dev libreadline-dev libhidapi-dev libusb-1.0.0-dev qttools5-dev libcurl4-openssl-dev lsb-release openssh-client'
