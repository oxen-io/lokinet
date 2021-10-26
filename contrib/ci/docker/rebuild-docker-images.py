#!/usr/bin/env python3

import subprocess
import tempfile
import optparse
import sys
from concurrent.futures import ThreadPoolExecutor

parser = optparse.OptionParser()
parser.add_option("--no-cache", action="store_true",
                  help="Run `docker build` with the `--no-cache` option to ignore existing images")
parser.add_option("--parallel", "-j", type="int", default=1,
                  help="Run up to this many builds in parallel")
(options, args) = parser.parse_args()

registry_base = 'registry.oxen.rocks/lokinet-ci-'

distros = [*(('debian', x) for x in ('sid', 'stable', 'testing', 'bullseye', 'buster')),
           *(('ubuntu', x) for x in ('rolling', 'lts', 'impish', 'hirsute', 'focal', 'bionic'))]

manifests = {}  # "image:latest": ["image/amd64", "image/arm64v8", ...]


def arches(distro):
    a = ['amd64', 'arm64v8', 'arm32v7']
    if distro[0] == 'debian' or distro == ('ubuntu', 'bionic'):
        a.append('i386')  # i386 builds don't work on later ubuntu
    return a


failure = False


def run_or_report(*args):
    try:
        subprocess.run(
            args, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding='utf8')
    except subprocess.CalledProcessError as e:
        with tempfile.NamedTemporaryFile(suffix=".log", delete=False) as log:
            log.write(e.output.encode())
            global failure
            failure = True
            print("""
\033[31;1mAn error occured ({}) running:
    {}
See {} for the command log.\033[0m
""".format(e, ' '.join(args), log.name), file=sys.stderr)
            raise e


def build_tag(tag_base, arch, contents):
    if failure:
        raise ChildProcessError()
    with tempfile.NamedTemporaryFile() as dockerfile:
        dockerfile.write(contents.encode())
        dockerfile.flush()

        tag = '{}/{}'.format(tag_base, arch)
        print("\033[33;1mrebuilding \033[35;1m{}\033[0m".format(tag))
        run_or_report('docker', 'build', '--pull', '-f', dockerfile.name, '-t', tag,
                      *(('--no-cache',) if options.no_cache else ()), '.')
        print("\033[33;1mpushing \033[35;1m{}\033[0m".format(tag))
        run_or_report('docker', 'push', tag)
        print("\033[32;1mFinished \033[35;1m{}\033[0m".format(tag))

        latest = tag_base + ':latest'
        global manifests
        if latest in manifests:
            manifests[latest].append(tag)
        else:
            manifests[latest] = [tag]


def base_distro_build(distro, arch):
    tag = '{r}{distro[0]}-{distro[1]}-base'.format(r=registry_base, distro=distro)
    codename = 'latest' if distro == ('ubuntu', 'lts') else distro[1]
    build_tag(tag, arch, """
FROM {}/{}:{}
RUN /bin/bash -c 'echo "man-db man-db/auto-update boolean false" | debconf-set-selections'
RUN apt-get -o=Dpkg::Use-Pty=0 -q update \
    && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y \
    && apt-get -o=Dpkg::Use-Pty=0 -q autoremove -y
    """.format(arch, distro[0], codename))


def distro_build(distro, arch):
    prefix = '{r}{distro[0]}-{distro[1]}'.format(r=registry_base, distro=distro)
    fmtargs = dict(arch=arch, distro=distro, prefix=prefix)

    # (distro)-(codename)-base: Base image from upstream: we sync the repos, but do nothing else.
    if (distro, arch) != (('debian', 'stable'), 'amd64'):  # debian-stable-base/amd64 already built
        base_distro_build(distro, arch)

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


# Android and flutter builds on top of debian-stable-base and adds a ton of android crap; we
# schedule this job as soon as the debian-sid-base/amd64 build finishes, because they easily take
# the longest and are by far the biggest images.
def android_builds():
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

    build_tag(registry_base + 'flutter', 'amd64', """
FROM {r}android
RUN cd /opt \
    && curl https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_2.2.2-stable.tar.xz \
        | tar xJv \
    && ln -s /opt/flutter/bin/flutter /usr/local/bin/ \
    && flutter precache
""".format(r=registry_base))


# lint is a tiny build (on top of debian-stable-base) with just formatting checking tools
def lint_build():
    build_tag(registry_base + 'lint', 'amd64', """
FROM {r}debian-stable-base
RUN apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y \
    clang-format-11 \
    eatmydata \
    git \
    jsonnet
""".format(r=registry_base))


def nodejs_build():
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


# Start debian-stable-base/amd64 on its own, because other builds depend on it and we want to get
# those (especially android/flutter) fired off as soon as possible (because it's slow and huge).
base_distro_build(['debian', 'stable'], 'amd64')

executor = ThreadPoolExecutor(max_workers=max(options.parallel, 1))

jobs = [executor.submit(b) for b in (android_builds, lint_build, nodejs_build)]

for d in distros:
    for a in arches(distros):
        jobs.append(executor.submit(distro_build, d, a))
while len(jobs):
    j = jobs.pop(0)
    try:
        j.result()
    except (ChildProcessError, subprocess.CalledProcessError):
        for k in jobs:
            k.cancel()


if failure:
    print("Error(s) occured, aborting!", file=sys.stderr)
    sys.exit(1)


for latest, tags in manifests.items():
    print("\033[32;1mpushing new manifest for \033[33;1m{}[\033[35;1m{}\033[33;1m]\033[0m".format(
          latest, ', '.join(tags)))

    subprocess.run(['docker', 'manifest', 'rm', latest], stderr=subprocess.DEVNULL, check=False)
    run_or_report('docker', 'manifest', 'create', latest, *tags)
    run_or_report('docker', 'manifest', 'push', latest)
