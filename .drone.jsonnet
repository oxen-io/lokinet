local default_deps_base='libsystemd-dev python3-dev libcurl4-openssl-dev libuv1-dev libunbound-dev nettle-dev libssl-dev libevent-dev libsqlite3-dev';
local default_deps_nocxx='libsodium-dev ' + default_deps_base; // libsodium-dev needs to be >= 1.0.18
local default_deps='g++ ' + default_deps_nocxx; // g++ sometimes needs replacement
local default_windows_deps='mingw-w64-binutils mingw-w64-gcc mingw-w64-crt mingw-w64-headers mingw-w64-winpthreads perl openssh zip bash'; // deps for windows cross compile



local submodules = {
    name: 'submodules',
    image: 'drone/git',
    commands: ['git fetch --tags', 'git submodule update --init --recursive --depth=1']
};

// Regular build on a debian-like system:
local debian_pipeline(name, image,
        arch='amd64',
        deps=default_deps,
        build_type='Release',
        lto=false,
        werror=true,
        cmake_extra='',
        extra_cmds=[],
        imaginary_repo=false,
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
                'echo "man-db man-db/auto-update boolean false" | debconf-set-selections',
                'apt-get update',
                'apt-get install -y eatmydata',
                'eatmydata apt-get dist-upgrade -y',
                ] + (if imaginary_repo then [
                    'eatmydata apt-get install -y gpg curl lsb-release',
                    'echo deb https://deb.imaginary.stream $$(lsb_release -sc) main >/etc/apt/sources.list.d/imaginary.stream.list',
                    'curl -s https://deb.imaginary.stream/public.gpg | apt-key add -',
                    'eatmydata apt-get update'
                    ] else []
                ) + [
                'eatmydata apt-get install -y gdb cmake git ninja-build pkg-config ccache ' + deps,
                'mkdir build',
                'cd build',
                'cmake .. -G Ninja -DCMAKE_CXX_FLAGS=-fdiagnostics-color=always -DCMAKE_BUILD_TYPE='+build_type+' ' +
                    (if werror then '-DWARNINGS_AS_ERRORS=ON ' else '') +
                    (if lto then '' else '-DWITH_LTO=OFF ') +
                cmake_extra,
                'ninja -v',
                '../contrib/ci/drone-gdb.sh ./test/testAll --gtest_color=yes',
                '../contrib/ci/drone-gdb.sh ./test/catchAll --use-colour yes',
            ] + extra_cmds,
        }
    ],
};

// windows cross compile on alpine linux
local windows_cross_pipeline(name, image,
        arch='amd64',
        deps=default_windows_deps,
        build_type='Release',
        lto=false,
        werror=false,
        cmake_extra='',
        toolchain='32',
        extra_cmds=[],
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
                'apk update && apk upgrade',
                'apk add cmake git ninja pkgconf ccache patch make ' + deps,
                'git clone https://github.com/despair86/libuv.git win32-setup/libuv',
                'mkdir build',
                'cd build',
                'cmake .. -G Ninja -DCMAKE_CROSSCOMPILE=ON -DCMAKE_EXE_LINKER_FLAGS=-fstack-protector -DLIBUV_ROOT=$PWD/../win32-setup/libuv -DCMAKE_CXX_FLAGS=-fdiagnostics-color=always -DCMAKE_TOOLCHAIN_FILE=../contrib/cross/mingw'+toolchain+'.cmake -DCMAKE_BUILD_TYPE='+build_type+' ' +
                    (if werror then '-DWARNINGS_AS_ERRORS=ON ' else '') +
                    (if lto then '' else '-DWITH_LTO=OFF ') +
                    "-DBUILD_STATIC_DEPS=ON -DDOWNLOAD_SODIUM=ON -DBUILD_PACKAGE=OFF -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=ON -DNATIVE_BUILD=OFF -DSTATIC_LINK=ON" +
                cmake_extra,
                'ninja -v',
            ] + extra_cmds,
        }
    ],
};

// Builds a snapshot .deb on a debian-like system by merging into the debian/* or ubuntu/* branch
local deb_builder(image, distro, distro_branch, arch='amd64', imaginary_repo=false) = {
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
                'echo "man-db man-db/auto-update boolean false" | debconf-set-selections',
                'apt-get update',
                'apt-get install -y eatmydata',
                'eatmydata apt-get install -y git devscripts equivs ccache git-buildpackage python3-dev' + (if imaginary_repo then ' gpg' else'')
                ] + (if imaginary_repo then [ // Some distros need the imaginary.stream repo for backported sodium, etc.
                    'echo deb https://deb.imaginary.stream $${distro} main >/etc/apt/sources.list.d/imaginary.stream.list',
                    'curl -s https://deb.imaginary.stream/public.gpg | apt-key add -',
                    'eatmydata apt-get update'
                ] else []) + [
                |||
                    # Look for the debian branch in this repo first, try upstream if that fails.
                    if ! git checkout $${distro_branch}; then
                        git remote add --fetch upstream https://github.com/loki-project/loki-network.git &&
                        git checkout $${distro_branch}
                    fi
                |||,
                'git merge ${DRONE_COMMIT}',
                'export DEBEMAIL="${DRONE_COMMIT_AUTHOR_EMAIL}" DEBFULLNAME="${DRONE_COMMIT_AUTHOR_NAME}"',
                'gbp dch -S -s "HEAD^" --spawn-editor=never -U low',
                'eatmydata mk-build-deps --install --remove --tool "apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends -y"',
                'export DEB_BUILD_OPTIONS="parallel=$$(nproc)"',
                'grep -q lib debian/lokinet-bin.install || echo "/usr/lib/lib*.so*" >>debian/lokinet-bin.install',
                'debuild -e CCACHE_DIR -b',
                'pwd',
                'find ./contrib/ci',
                './contrib/ci/drone-debs-upload.sh ' + distro,
            ]
        }
    ]
};

// Macos build
local mac_builder(name, build_type='Release', werror=true, cmake_extra='', extra_cmds=[], allow_fail=false) = {
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
                // If you don't do this then the C compiler doesn't have an include path containing
                // basic system headers.  WTF apple:
                'export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"',
                'mkdir build',
                'cd build',
                'cmake .. -G Ninja -DCMAKE_CXX_FLAGS=-fcolor-diagnostics -DCMAKE_BUILD_TYPE='+build_type+' ' +
                    (if werror then '-DWARNINGS_AS_ERRORS=ON ' else '') + cmake_extra,
                'ninja -v',
                './test/testAll --gtest_color=yes',
                './test/catchAll --use-colour yes',
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
            name: 'build', image: 'debian:sid',
            commands: [
                'apt-get update', 'apt-get install -y eatmydata',
                'eatmydata apt-get install -y git clang-format-9',
                './contrib/ci/drone-format-verify.sh']
        }]
    },

    // Various debian builds
    debian_pipeline("Debian sid (amd64)", "debian:sid", lto=true),
    debian_pipeline("Debian sid/Debug (amd64)", "debian:sid", build_type='Debug', lto=true),
    debian_pipeline("Debian sid/clang-10 (amd64)", "debian:sid", deps='clang-10 '+default_deps_nocxx,
                    cmake_extra='-DCMAKE_C_COMPILER=clang-10 -DCMAKE_CXX_COMPILER=clang++-10 ', lto=true),
    debian_pipeline("Debian buster (i386)", "i386/debian:buster", cmake_extra='-DDOWNLOAD_SODIUM=ON'),
    debian_pipeline("Ubuntu focal (amd64)", "ubuntu:focal"),
    debian_pipeline("Ubuntu bionic (amd64)", "ubuntu:bionic", deps='g++-8 ' + default_deps_nocxx,
                    cmake_extra='-DCMAKE_C_COMPILER=gcc-8 -DCMAKE_CXX_COMPILER=g++-8', imaginary_repo=true),

    // ARM builds (ARM64 and armhf)
    debian_pipeline("Debian sid (ARM64)", "debian:sid", arch="arm64"),
    debian_pipeline("Debian buster (armhf)", "arm32v7/debian:buster", arch="arm64", cmake_extra='-DDOWNLOAD_SODIUM=ON'),
    
    // Windows builds (WOW64 and native)
    windows_cross_pipeline("win32 on alpine (amd64)", "alpine:edge",
        toolchain='64', extra_cmds=[
          '../contrib/ci/drone-static-upload.sh'
    ]),
     windows_cross_pipeline("win32 on alpine (i386)", "i386/alpine:edge",
        toolchain='32', extra_cmds=[
          '../contrib/ci/drone-static-upload.sh'
    ]),

    // Static build (on bionic) which gets uploaded to builds.lokinet.dev:
    debian_pipeline("Static (bionic amd64)", "ubuntu:bionic", deps='g++-8 python3-dev', lto=true,
                    cmake_extra='-DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON -DCMAKE_C_COMPILER=gcc-8 -DCMAKE_CXX_COMPILER=g++-8 ' +
                        '-DDOWNLOAD_SODIUM=ON -DDOWNLOAD_CURL=ON -DDOWNLOAD_UV=ON -DWITH_SYSTEMD=OFF',
                    extra_cmds=[
                        '../contrib/ci/drone-check-static-libs.sh',
                        '../contrib/ci/drone-static-upload.sh'
                    ]),

    // integration tests
    debian_pipeline("Router Hive", "ubuntu:focal", deps='python3-dev python3-pytest python3-pybind11 ' + default_deps,
                    cmake_extra='-DWITH_HIVE=ON'),

    // Deb builds:
    deb_builder("debian:sid", "sid", "debian/sid"),
    deb_builder("debian:buster", "buster", "debian/buster", imaginary_repo=true),
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
