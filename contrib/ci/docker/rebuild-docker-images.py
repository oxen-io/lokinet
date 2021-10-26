#!/usr/bin/env python3

import subprocess
import tempfile

no_cache = True  # Whether to build with --no-cache

registry_base = 'registry.oxen.rocks/lokinet-ci-'

distros = [*(['debian', x] for x in ('sid', 'stable', 'testing', 'bullseye', 'buster')),
           *(['ubuntu', x] for x in ('rolling', 'lts', 'impish', 'hirsute', 'focal', 'bionic'))]

manifests = {}  # "image:latest": ["image/amd64", "image/arm64v8", ...]


def arches(distro):
    a = ['amd64', 'arm64v8', 'arm32v7']
    if distro[0] == 'debian' or distro == ['ubuntu', 'bionic']:
        a.append('i386')  # i386 builds don't work on later ubuntu
    return a


def build_tag(tag_base, arch, contents):
    with tempfile.NamedTemporaryFile() as dockerfile:
        dockerfile.write(contents.encode())
        dockerfile.flush()

        tag = '{}/{}'.format(tag_base, arch)
        print("\033[32;1mrebuilding \033[35;1m{}\033[0m".format(tag))
        subprocess.run(['docker', 'build', '--pull', '-f', dockerfile.name, '-t', tag,
                        *(('--no-cache',) if no_cache else ()), '.'],
                       check=True)
        subprocess.run(['docker', 'push', tag], check=True)

        latest = tag_base + ':latest'
        if latest in manifests:
            manifests[latest].append(tag)
        else:
            manifests[latest] = [tag]


def distro_build(distro, arch):
    prefix = '{r}{distro[0]}-{distro[1]}'.format(r=registry_base, distro=distro)
    fmtargs = dict(arch=arch, distro=distro, prefix=prefix)

    # (distro)-(codename)-base: Base image from upstream: we sync the repos, but do nothing else.
    build_tag(prefix + '-base', arch, """
FROM {arch}/{distro[0]}:{codename}
RUN /bin/bash -c 'echo "man-db man-db/auto-update boolean false" | debconf-set-selections'
RUN apt-get -o=Dpkg::Use-Pty=0 -q update \
    && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y \
    && apt-get -o=Dpkg::Use-Pty=0 -q autoremove -y
""".format(**fmtargs, codename='latest' if distro == ['ubuntu', 'lts'] else distro[1]))

    # (distro)-(codename)-builder: Deb builder image used for building debs; we add the basic tools
    # we use to build debs, not including things that should come from the dependencies in the
    # debian/control file.
    build_tag(prefix + '-builder', arch, """
FROM {prefix}-base/{arch}
RUN apt-get -o=Dpkg::Use-Pty=0 -q update \
    && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y \
    && apt-get -o=Dpkg::Use-Pty=0 --no-install-recommends -q install -y \
        ccache \
        devscripts \
        equivs \
        g++ \
        git \
        git-buildpackage \
        openssh-client
""".format(**fmtargs))

    # (distro)-(codename): Basic image we use for most builds.  This takes the -builder and adds
    # most dependencies found in our packages.
    build_tag(prefix, arch, """
FROM {prefix}-builder/{arch}
RUN apt-get -o=Dpkg::Use-Pty=0 -q update \
    && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y \
    && apt-get -o=Dpkg::Use-Pty=0 --no-install-recommends -q install -y \
        automake \
        ccache \
        cmake \
        eatmydata \
        g++ \
        gdb \
        git \
        libboost-program-options-dev \
        libboost-serialization-dev \
        libboost-thread-dev \
        libcurl4-openssl-dev \
        libevent-dev \
        libgtest-dev \
        libhidapi-dev \
        libjemalloc-dev \
        libminiupnpc-dev \
        libreadline-dev \
        libsodium-dev \
        libsqlite3-dev \
        libssl-dev \
        libsystemd-dev \
        libtool \
        libunbound-dev \
        libunwind8-dev \
        libusb-1.0.0-dev \
        libuv1-dev \
        libzmq3-dev \
        lsb-release \
        make \
        nettle-dev \
        ninja-build \
        openssh-client \
        patch \
        pkg-config \
        pybind11-dev \
        python3-dev \
        python3-pip \
        python3-pybind11 \
        python3-pytest \
        python3-setuptools \
        qttools5-dev
""".format(**fmtargs))


for d in distros:
    for a in arches(distros):
        distro_build(d, a)


# Other images:

# lint is a tiny build with just formatting checking tools

build_tag(registry_base + 'lint', 'amd64', """
FROM {r}debian-stable-base
RUN apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y \
    clang-format-11 \
    eatmydata \
    git \
    jsonnet
""".format(r=registry_base))

# nodejs
build_tag(registry_base + 'nodejs', 'amd64', """
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
""")


# Android builds on debian-stable-base and adds a ton of android crap:
build_tag(registry_base + 'android', 'amd64', """
FROM {r}debian-stable-base
RUN /bin/bash -c 'sed -i "s/main/main contrib/g" /etc/apt/sources.list'
RUN apt-get -o=Dpkg::Use-Pty=0 -q update \
    && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y \
    && apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y \
        android-sdk \
        automake \
        ccache \
        cmake \
        curl \
        git \
        google-android-ndk-installer \
        libtool \
        make \
        openssh-client \
        patch \
        pkg-config \
        wget \
        xz-utils \
        zip \
    && git clone https://github.com/Shadowstyler/android-sdk-licenses.git /tmp/android-sdk-licenses \
    && cp -a /tmp/android-sdk-licenses/*-license /usr/lib/android-sdk/licenses \
    && rm -rf /tmp/android-sdk-licenses
""".format(r=registry_base))


# Flutter image takes android and adds even more crap:
build_tag(registry_base + 'flutter', 'amd64', """
FROM {r}android
RUN cd /opt \
    && curl https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_2.2.2-stable.tar.xz \
        | tar xJv \
    && ln -s /opt/flutter/bin/flutter /usr/local/bin/ \
    && flutter precache
""".format(r=registry_base))


# debian-sid-clang adds clang + libc++ to debian-sid (amd64 only)
build_tag(registry_base + 'debian-sid-clang', 'amd64', """
FROM {r}debian-sid
RUN apt-get -o=Dpkg::Use-Pty=0 -q update \
    && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y \
    && apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y \
        clang-13 \
        libc++-13-dev \
        libc++abi-13-dev \
        lld-13
""".format(r=registry_base))


# debian-win32-cross is debian-testing-base + stuff for compiling windows binaries
build_tag(registry_base + 'debian-win32-cross', 'amd64', """
FROM {r}debian-testing-base
RUN apt-get -o=Dpkg::Use-Pty=0 -q update \
    && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y \
    && apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y \
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
""".format(r=registry_base))


for latest, tags in manifests.items():
    print("\033[32;1mpushing new manifest for \033[33;1m{}[\033[35;1m{}\033[33;1m]\033[0m".format(
          latest, ', '.join(tags)))

    subprocess.run(['manifest', 'rm', latest], stderr=DEVNULL, check=False)
    subprocess.run(['manifest', 'create', latest, *tags], check=True)
    subprocess.run(['manifest', 'push', latest], check=True)
