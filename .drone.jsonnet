local default_deps_base='libsystemd-dev python3-dev libuv1-dev libunbound-dev nettle-dev libssl-dev libevent-dev libsqlite3-dev libcurl4-openssl-dev make';
local default_deps_nocxx='libsodium-dev ' + default_deps_base; // libsodium-dev needs to be >= 1.0.18
local default_deps='g++ ' + default_deps_nocxx;
local default_windows_deps='mingw-w64 zip nsis';
local docker_base = 'registry.oxen.rocks/lokinet-ci-';

local submodules = {
    name: 'submodules',
    image: 'drone/git',
    commands: ['git fetch --tags', 'git submodule update --init --recursive --depth=1']
};

local apt_get_quiet = 'apt-get -o=Dpkg::Use-Pty=0 -q';

// Regular build on a debian-like system:
local debian_pipeline(name, image,
        arch='amd64',
        deps=default_deps,
        build_type='Release',
        lto=false,
        werror=true,
        cmake_extra='',
        extra_cmds=[],
        jobs=6,
        loki_repo=false,
        allow_fail=false) = {
    kind: 'pipeline',
    type: 'docker',
    name: name,
    platform: { arch: arch },
    trigger: { branch: { exclude: ['debian/*', 'ubuntu/*'] } },
    steps: [
        submodules,
        {
            name: 'build',
            image: image,
            [if allow_fail then "failure"]: "ignore",
            environment: { SSH_KEY: { from_secret: "SSH_KEY" } },
            commands: [
                'echo "Building on ${DRONE_STAGE_MACHINE}"',
                'echo "man-db man-db/auto-update boolean false" | debconf-set-selections',
                apt_get_quiet + ' update',
                apt_get_quiet + ' install -y eatmydata',
                ] + (if loki_repo then [
                    'eatmydata ' + apt_get_quiet + ' install -y lsb-release',
                    'cp contrib/deb.loki.network.gpg /etc/apt/trusted.gpg.d',
                    'echo deb http://deb.loki.network $$(lsb_release -sc) main >/etc/apt/sources.list.d/loki.network.list',
                    'eatmydata ' + apt_get_quiet + ' update'
                    ] else []
                ) + [
                'eatmydata ' + apt_get_quiet + ' dist-upgrade -y',
                'eatmydata ' + apt_get_quiet + ' install -y gdb cmake git pkg-config ccache ' + deps,
                'mkdir build',
                'cd build',
                'cmake .. -DWITH_SETCAP=OFF -DCMAKE_CXX_FLAGS=-fdiagnostics-color=always -DCMAKE_BUILD_TYPE='+build_type+' ' +
                    (if werror then '-DWARNINGS_AS_ERRORS=ON ' else '') +
                    '-DWITH_LTO=' + (if lto then 'ON ' else 'OFF ') +
                cmake_extra,
                'make -j' + jobs,
                '../contrib/ci/drone-gdb.sh ./test/testAll --use-colour yes',
            ] + extra_cmds,
        }
    ],
};
local apk_builder(name, image, extra_cmds=[], allow_fail=false, jobs=6) = {
    kind: 'pipeline',
    type: 'docker',
    name: name,
    platform: {arch: "amd64"},
    trigger: { branch: { exclude: ['debian/*', 'ubuntu/*'] } },
    steps: [
        submodules,
        {
            name: 'build',
            image: image,
            [if allow_fail then "failure"]: "ignore",
            environment: { SSH_KEY: { from_secret: "SSH_KEY" }, ANDROID: "android" },
            commands: [
                'JOBS='+jobs+' NDK=/usr/lib/android-ndk ./contrib/android.sh'
            ] + extra_cmds
        }
    ]
};
// windows cross compile on debian
local windows_cross_pipeline(name, image,
        arch='amd64',
        build_type='Release',
        lto=false,
        werror=false,
        cmake_extra='',
        toolchain='32',
        extra_cmds=[],
        jobs=6,
        allow_fail=false) = {
    kind: 'pipeline',
    type: 'docker',
    name: name,
    platform: { arch: arch },
    trigger: { branch: { exclude: ['debian/*', 'ubuntu/*'] } },
    steps: [
        submodules,
        {
            name: 'build',
            image: image,
            [if allow_fail then "failure"]: "ignore",
            environment: { SSH_KEY: { from_secret: "SSH_KEY" }, WINDOWS_BUILD_NAME: toolchain+"bit" },
            commands: [
                'echo "Building on ${DRONE_STAGE_MACHINE}"',
                'echo "man-db man-db/auto-update boolean false" | debconf-set-selections',
                apt_get_quiet + ' update',
                apt_get_quiet + ' install -y eatmydata',
                'eatmydata ' + apt_get_quiet + ' install -y build-essential cmake git pkg-config ccache g++-mingw-w64-x86-64-posix nsis zip automake libtool',
                'update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix',
                'update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix',
                'JOBS=' + jobs + ' ./contrib/windows.sh'
            ] + extra_cmds,
        }
    ],
};

// Builds a snapshot .deb on a debian-like system by merging into the debian/* or ubuntu/* branch
local deb_builder(image, distro, distro_branch, arch='amd64', loki_repo=true) = {
    kind: 'pipeline',
    type: 'docker',
    name: 'DEB (' + distro + (if arch == 'amd64' then '' else '/' + arch) + ')',
    platform: { arch: arch },
    environment: { distro_branch: distro_branch, distro: distro },
    steps: [
        submodules,
        {
            name: 'build',
            image: image,
            failure: 'ignore',
            environment: { SSH_KEY: { from_secret: "SSH_KEY" } },
            commands: [
                'echo "Building on ${DRONE_STAGE_MACHINE}"',
                'echo "man-db man-db/auto-update boolean false" | debconf-set-selections'
                ] + (if loki_repo then [
                    'cp contrib/deb.loki.network.gpg /etc/apt/trusted.gpg.d',
                    'echo deb http://deb.loki.network $${distro} main >/etc/apt/sources.list.d/loki.network.list'
                ] else []) + [
                apt_get_quiet + ' update',
                apt_get_quiet + ' install -y eatmydata',
                'eatmydata ' + apt_get_quiet + ' install -y git devscripts equivs ccache git-buildpackage python3-dev',
                |||
                    # Look for the debian branch in this repo first, try upstream if that fails.
                    if ! git checkout $${distro_branch}; then
                        git remote add --fetch upstream https://github.com/oxen-io/loki-network.git &&
                        git checkout $${distro_branch}
                    fi
                |||,
                # Tell the merge how to resolve conflicts in the source .drone.jsonnet (we don't
                # care about it at all since *this* .drone.jsonnet is already loaded).
                'git config merge.ours.driver true',
                'echo .drone.jsonnet merge=ours >>.gitattributes',

                'git merge ${DRONE_COMMIT}',
                'export DEBEMAIL="${DRONE_COMMIT_AUTHOR_EMAIL}" DEBFULLNAME="${DRONE_COMMIT_AUTHOR_NAME}"',
                'gbp dch -S -s "HEAD^" --spawn-editor=never -U low',
                'eatmydata mk-build-deps --install --remove --tool "' + apt_get_quiet + ' -o Debug::pkgProblemResolver=yes --no-install-recommends -y"',
                'export DEB_BUILD_OPTIONS="parallel=$$(nproc)"',
                #'grep -q lib debian/lokinet-bin.install || echo "/usr/lib/lib*.so*" >>debian/lokinet-bin.install',
                'debuild -e CCACHE_DIR -b',
                './contrib/ci/drone-debs-upload.sh ' + distro,
            ]
        }
    ]
};


// Macos build
local mac_builder(name,
        build_type='Release',
        werror=true,
        cmake_extra='',
        extra_cmds=[],
        jobs=6,
        allow_fail=false) = {
    kind: 'pipeline',
    type: 'exec',
    name: name,
    platform: { os: 'darwin', arch: 'amd64' },
    steps: [
        { name: 'submodules', commands: ['git fetch --tags', 'git submodule update --init --recursive'] },
        {
            name: 'build',
            environment: { SSH_KEY: { from_secret: "SSH_KEY" } },
            commands: [
                'echo "Building on ${DRONE_STAGE_MACHINE}"',
                // If you don't do this then the C compiler doesn't have an include path containing
                // basic system headers.  WTF apple:
                'export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"',
                'ulimit -n 1024', // because macos sets ulimit to 256 for some reason yeah idk
                'mkdir build',
                'cd build',
                'cmake .. -DCMAKE_CXX_FLAGS=-fcolor-diagnostics -DCMAKE_BUILD_TYPE='+build_type+' ' +
                    (if werror then '-DWARNINGS_AS_ERRORS=ON ' else '') + cmake_extra,
                'make -j' + jobs,
                './test/testAll --use-colour yes',
            ] + extra_cmds,
        }
    ]
};


[
    {
        name: 'lint check',
        kind: 'pipeline',
        type: 'docker',
        steps: [{
            name: 'build', image: 'registry.oxen.rocks/lokinet-ci-lint',
            commands: [
                'echo "Building on ${DRONE_STAGE_MACHINE}"',
                apt_get_quiet + ' update',
                apt_get_quiet + ' install -y eatmydata',
                'eatmydata ' + apt_get_quiet + ' install -y git clang-format-11',
                './contrib/ci/drone-format-verify.sh']
        }]
    },

    // Various debian builds
    debian_pipeline("Debian sid (amd64)", "debian:sid"),
    debian_pipeline("Debian sid/Debug (amd64)", "debian:sid", build_type='Debug'),
    debian_pipeline("Debian sid/clang-11 (amd64)", docker_base+'debian-sid', deps='clang-11 '+default_deps_nocxx,
                    cmake_extra='-DCMAKE_C_COMPILER=clang-11 -DCMAKE_CXX_COMPILER=clang++-11 '),
    debian_pipeline("Debian buster (i386)", "i386/debian:buster", cmake_extra='-DDOWNLOAD_SODIUM=ON'),
    debian_pipeline("Ubuntu focal (amd64)", docker_base+'ubuntu-focal'),
    debian_pipeline("Ubuntu bionic (amd64)", "ubuntu:bionic", deps='g++-8 ' + default_deps_nocxx,
                    cmake_extra='-DCMAKE_C_COMPILER=gcc-8 -DCMAKE_CXX_COMPILER=g++-8', loki_repo=true),

    // ARM builds (ARM64 and armhf)
    debian_pipeline("Debian sid (ARM64)", "debian:sid", arch="arm64", jobs=4),
    debian_pipeline("Debian buster (armhf)", "arm32v7/debian:buster", arch="arm64", cmake_extra='-DDOWNLOAD_SODIUM=ON', jobs=4),
    // Static armhf build (gets uploaded)
    debian_pipeline("Static (buster armhf)", "arm32v7/debian:buster", arch="arm64", deps='g++ python3-dev automake libtool',
                    cmake_extra='-DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON ' +
                        '-DCMAKE_CXX_FLAGS="-march=armv7-a+fp" -DCMAKE_C_FLAGS="-march=armv7-a+fp" -DNATIVE_BUILD=OFF ' +
                        '-DWITH_SYSTEMD=OFF',
                    extra_cmds=[
                        '../contrib/ci/drone-check-static-libs.sh',
                        'UPLOAD_OS=linux-armhf ../contrib/ci/drone-static-upload.sh'
                    ],
                    jobs=4),
    // android apk builder
    apk_builder("android apk", "registry.oxen.rocks/lokinet-ci-android", extra_cmds=['UPLOAD_OS=android ./contrib/ci/drone-static-upload.sh']),

    // Windows builds (x64)
    windows_cross_pipeline("Windows (amd64)", docker_base+'debian-win32-cross',
        toolchain='64', extra_cmds=[
          './contrib/ci/drone-static-upload.sh'
    ]),

    // Static build (on bionic) which gets uploaded to builds.lokinet.dev:
    debian_pipeline("Static (bionic amd64)", docker_base+'ubuntu-bionic', deps='g++-8 python3-dev automake libtool', lto=true,
                    cmake_extra='-DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON -DCMAKE_C_COMPILER=gcc-8 -DCMAKE_CXX_COMPILER=g++-8 ' +
                        '-DCMAKE_CXX_FLAGS="-march=x86-64 -mtune=haswell" -DCMAKE_C_FLAGS="-march=x86-64 -mtune=haswell" -DNATIVE_BUILD=OFF ' +
                        '-DWITH_SYSTEMD=OFF',
                    extra_cmds=[
                        '../contrib/ci/drone-check-static-libs.sh',
                        '../contrib/ci/drone-static-upload.sh'
                    ]),

    // integration tests
    debian_pipeline("Router Hive", "ubuntu:focal", deps='python3-dev python3-pytest python3-pybind11 ' + default_deps,
                    cmake_extra='-DWITH_HIVE=ON'),

    // Deb builds:
    deb_builder("debian:sid", "sid", "debian/sid"),
    deb_builder("debian:buster", "buster", "debian/buster"),
    deb_builder("ubuntu:focal", "focal", "ubuntu/focal"),
    deb_builder("debian:sid", "sid", "debian/sid", arch='arm64'),

    // Macos builds:
    mac_builder('macOS (Release)'),
    mac_builder('macOS (Debug)', build_type='Debug'),
    mac_builder('macOS (Static)', cmake_extra='-DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON -DDOWNLOAD_SODIUM=FORCE -DDOWNLOAD_CURL=FORCE -DDOWNLOAD_UV=FORCE',
                extra_cmds=[
                    '../contrib/ci/drone-check-static-libs.sh',
                    '../contrib/ci/drone-static-upload.sh'
                ]),
]
