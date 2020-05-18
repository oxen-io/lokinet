local default_deps_base='libsystemd-dev python3-dev libcurl4-openssl-dev libuv1-dev';
local default_deps_nocxx='libsodium-dev ' + default_deps_base; // libsodium-dev needs to be >= 1.0.18
local default_deps='g++ ' + default_deps_nocxx; // g++ sometimes needs replacement

local debian_pipeline(name, image, arch='amd64', deps=default_deps, build_type='Release', werror=true, cmake_extra='', extra_cmds=[], allow_fail=false) = {
    kind: 'pipeline',
    type: 'docker',
    name: name,
    platform: { arch: arch },
    environment: { CLICOLOR_FORCE: '1' }, // Lets color through ninja (1.9+)
    steps: [
        {
            name: 'build',
            image: image,
            [if allow_fail then "failure"]: "ignore",
            commands: [
                'apt-get update',
                'apt-get install -y eatmydata',
                'eatmydata apt-get dist-upgrade -y',
                'eatmydata apt-get install -y cmake git ninja-build pkg-config ccache ' + deps,
                'git submodule update --init --recursive',
                'mkdir build',
                'cd build',
                'cmake .. -G Ninja -DCMAKE_CXX_FLAGS=-fdiagnostics-color=always -DCMAKE_BUILD_TYPE='+build_type+' ' +
                    (if werror then '-DWARNINGS_AS_ERRORS=ON ' else '') +
                    cmake_extra,
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
                'eatmydata apt-get install -y make git clang-format-9',
                'make format-verify']
        }]
    },
    debian_pipeline("Debian sid (amd64)", "debian:sid"),
    debian_pipeline("Debian sid/Debug (amd64)", "debian:sid", build_type='Debug'),
    debian_pipeline("Debian sid/clang-10 (amd64)", "debian:sid", deps='clang-10 '+default_deps_nocxx,
                    cmake_extra='-DCMAKE_C_COMPILER=clang-10 -DCMAKE_CXX_COMPILER=clang++-10 '),
    debian_pipeline("Debian buster (amd64)", "debian:buster", cmake_extra='-DDOWNLOAD_SODIUM=ON'),
    debian_pipeline("Debian buster (i386)", "i386/debian:buster", cmake_extra='-DDOWNLOAD_SODIUM=ON'),
    debian_pipeline("Ubuntu focal (amd64)", "ubuntu:focal"),
    debian_pipeline("Ubuntu bionic (amd64)", "ubuntu:bionic", deps='g++-8 ' + default_deps_base,
                    cmake_extra='-DCMAKE_C_COMPILER=gcc-8 -DCMAKE_CXX_COMPILER=g++-8 -DDOWNLOAD_SODIUM=ON'),
    debian_pipeline("Ubuntu bionic/static (amd64)", "ubuntu:bionic", deps='g++-8 python3-dev',
                    cmake_extra='-DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON -DCMAKE_C_COMPILER=gcc-8 -DCMAKE_CXX_COMPILER=g++-8 ' +
                        '-DDOWNLOAD_SODIUM=ON -DDOWNLOAD_CURL=ON -DDOWNLOAD_UV=ON -DWITH_SYSTEMD=OFF',
                    extra_cmds=['if ldd daemon/lokinet | grep -ev "(linux-vdso|ld-linux-x86-64|lib(pthread|dl|stdc\\\\+\\\\+|gcc_s|c|m))\\\\.so; ' +
                                'then echo -e "\\\\e[31;1mlokinet links to unexpected libraries\\\\e[0m"; fi']),
    debian_pipeline("Ubuntu bionic (ARM64)", "ubuntu:bionic", arch="arm64", deps='g++-8 ' + default_deps_base,
                    cmake_extra='-DCMAKE_C_COMPILER=gcc-8 -DCMAKE_CXX_COMPILER=g++-8 -DDOWNLOAD_SODIUM=ON'),
    debian_pipeline("Debian sid (ARM64)", "debian:sid", arch="arm64"),
    debian_pipeline("Debian buster (armhf)", "arm32v7/debian:buster", arch="arm64", cmake_extra='-DDOWNLOAD_SODIUM=ON'),
]
