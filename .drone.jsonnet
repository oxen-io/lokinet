local default_deps_base = [
  'libsystemd-dev',
  'python3-dev',
  'libuv1-dev',
  'libunbound-dev',
  'nettle-dev',
  'libssl-dev',
  'libevent-dev',
  'libsqlite3-dev',
  'libcurl4-openssl-dev',
  'libzmq3-dev',
  'make',
];
local default_deps_nocxx = ['libsodium-dev'] + default_deps_base;  // libsodium-dev needs to be >= 1.0.18
local default_deps = ['g++'] + default_deps_nocxx;
local docker_base = 'registry.oxen.rocks/lokinet-ci-';

local submodule_commands = [
  'git fetch --tags',
  'git submodule update --init --recursive --depth=1 --jobs=4',
];
local submodules = {
  name: 'submodules',
  image: 'drone/git',
  commands: submodule_commands,
};

// cmake options for static deps mirror
local ci_dep_mirror(want_mirror) = (if want_mirror then ' -DLOCAL_MIRROR=https://oxen.rocks/deps ' else '');

local apt_get_quiet = 'apt-get -o=Dpkg::Use-Pty=0 -q';

// Regular build on a debian-like system:
local debian_pipeline(name,
                      image,
                      arch='amd64',
                      deps=default_deps,
                      build_type='Release',
                      lto=false,
                      werror=true,
                      cmake_extra='',
                      local_mirror=true,
                      extra_cmds=[],
                      jobs=6,
                      tests=true,
                      oxen_repo=false,
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
      pull: 'always',
      [if allow_fail then 'failure']: 'ignore',
      environment: { SSH_KEY: { from_secret: 'SSH_KEY' } },
      commands: [
                  'echo "Building on ${DRONE_STAGE_MACHINE}"',
                  'echo "man-db man-db/auto-update boolean false" | debconf-set-selections',
                  apt_get_quiet + ' update',
                  apt_get_quiet + ' install -y eatmydata',
                ] + (
                  if oxen_repo then [
                    'eatmydata ' + apt_get_quiet + ' install --no-install-recommends -y lsb-release',
                    'cp contrib/deb.oxen.io.gpg /etc/apt/trusted.gpg.d',
                    'echo deb http://deb.oxen.io $$(lsb_release -sc) main >/etc/apt/sources.list.d/oxen.list',
                    'eatmydata ' + apt_get_quiet + ' update',
                  ] else []
                ) + [
                  'eatmydata ' + apt_get_quiet + ' dist-upgrade -y',
                  'eatmydata ' + apt_get_quiet + ' install --no-install-recommends -y gdb cmake git pkg-config ccache ' + std.join(' ', deps),
                  'mkdir build',
                  'cd build',
                  'cmake .. -DWITH_SETCAP=OFF -DCMAKE_CXX_FLAGS=-fdiagnostics-color=always -DCMAKE_BUILD_TYPE=' + build_type + ' ' +
                  (if build_type == 'Debug' then ' -DWARN_DEPRECATED=OFF ' else '') +
                  (if werror then '-DWARNINGS_AS_ERRORS=ON ' else '') +
                  '-DWITH_LTO=' + (if lto then 'ON ' else 'OFF ') +
                  '-DWITH_TESTS=' + (if tests then 'ON ' else 'OFF ') +
                  cmake_extra +
                  ci_dep_mirror(local_mirror),
                  'VERBOSE=1 make -j' + jobs,
                  'cd ..',
                ]
                + (if tests then ['./contrib/ci/drone-gdb.sh ./build/test/testAll --use-colour yes'] else [])
                + extra_cmds,
    },
  ],
};
local apk_builder(name, image, extra_cmds=[], allow_fail=false, jobs=6) = {
  kind: 'pipeline',
  type: 'docker',
  name: name,
  platform: { arch: 'amd64' },
  trigger: { branch: { exclude: ['debian/*', 'ubuntu/*'] } },
  steps: [
    submodules,
    {
      name: 'build',
      image: image,
      pull: 'always',
      [if allow_fail then 'failure']: 'ignore',
      environment: { SSH_KEY: { from_secret: 'SSH_KEY' }, ANDROID: 'android' },
      commands: [
        'VERBOSE=1 JOBS=' + jobs + ' NDK=/usr/lib/android-ndk ./contrib/android.sh',
        'git clone https://github.com/oxen-io/lokinet-flutter-app lokinet-mobile',
        'cp -av build-android/out/* lokinet-mobile/lokinet_lib/android/src/main/jniLibs/',
        'cd lokinet-mobile',
        'flutter build apk --debug',
        'cd  ..',
        'cp lokinet-mobile/build/app/outputs/apk/debug/app-debug.apk lokinet.apk',
      ] + extra_cmds,
    },
  ],
};
// windows cross compile on debian
local windows_cross_pipeline(name,
                             image,
                             gui_image=docker_base + 'nodejs-lts',
                             arch='amd64',
                             build_type='Release',
                             lto=false,
                             werror=false,
                             cmake_extra='',
                             local_mirror=true,
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
      name: 'GUI',
      image: gui_image,
      pull: 'always',
      [if allow_fail then 'failure']: 'ignore',
      commands: [
        'echo "Building on ${DRONE_STAGE_MACHINE}"',
        'echo "man-db man-db/auto-update boolean false" | debconf-set-selections',
        apt_get_quiet + ' update',
        apt_get_quiet + ' install -y eatmydata',
        'eatmydata ' + apt_get_quiet + ' install --no-install-recommends -y p7zip-full wine',
        'cd gui',
        'yarn install --frozen-lockfile',
        'USE_SYSTEM_7ZA=true DISPLAY= WINEDEBUG=-all yarn win32',
      ],
    },
    {
      name: 'build',
      image: image,
      pull: 'always',
      [if allow_fail then 'failure']: 'ignore',
      environment: { SSH_KEY: { from_secret: 'SSH_KEY' }, WINDOWS_BUILD_NAME: 'x64' },
      commands: [
        'echo "Building on ${DRONE_STAGE_MACHINE}"',
        'echo "man-db man-db/auto-update boolean false" | debconf-set-selections',
        apt_get_quiet + ' update',
        apt_get_quiet + ' install -y eatmydata',
        'eatmydata ' + apt_get_quiet + ' install --no-install-recommends -y build-essential cmake git pkg-config ccache g++-mingw-w64-x86-64-posix nsis zip icoutils automake libtool librsvg2-bin bison',
        'JOBS=' + jobs + ' VERBOSE=1 ./contrib/windows.sh -DSTRIP_SYMBOLS=ON -DGUI_EXE=$${DRONE_WORKSPACE}/gui/release/Lokinet-GUI_portable.exe' +
        ci_dep_mirror(local_mirror),
      ] + extra_cmds,
    },
  ],
};

// linux cross compile on debian
local linux_cross_pipeline(name,
                           cross_targets,
                           arch='amd64',
                           build_type='Release',
                           cmake_extra='',
                           local_mirror=true,
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
      image: docker_base + 'debian-stable-cross',
      pull: 'always',
      [if allow_fail then 'failure']: 'ignore',
      environment: { SSH_KEY: { from_secret: 'SSH_KEY' }, CROSS_TARGETS: std.join(':', cross_targets) },
      commands: [
        'echo "Building on ${DRONE_STAGE_MACHINE}"',
        'VERBOSE=1 JOBS=' + jobs + ' ./contrib/cross.sh ' + std.join(' ', cross_targets) +
        ' -- ' + cmake_extra + ci_dep_mirror(local_mirror),
      ],
    },
  ],
};

// Builds a snapshot .deb on a debian-like system by merging into the debian/* or ubuntu/* branch
local deb_builder(image, distro, distro_branch, arch='amd64', oxen_repo=true) = {
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
      pull: 'always',
      failure: 'ignore',
      environment: { SSH_KEY: { from_secret: 'SSH_KEY' } },
      commands: [
        'echo "Building on ${DRONE_STAGE_MACHINE}"',
        'echo "man-db man-db/auto-update boolean false" | debconf-set-selections',
      ] + (if oxen_repo then [
             'cp contrib/deb.oxen.io.gpg /etc/apt/trusted.gpg.d',
             'echo deb http://deb.oxen.io $${distro} main >/etc/apt/sources.list.d/oxen.list',
           ] else []) + [
        apt_get_quiet + ' update',
        apt_get_quiet + ' install -y eatmydata',
        'eatmydata ' + apt_get_quiet + ' install --no-install-recommends -y git devscripts equivs ccache git-buildpackage python3-dev',
        |||
          # Look for the debian branch in this repo first, try upstream if that fails.
          if ! git checkout $${distro_branch}; then
              git remote add --fetch upstream https://github.com/oxen-io/lokinet.git &&
              git checkout $${distro_branch}
          fi
        |||,
        // Tell the merge how to resolve conflicts in the source .drone.jsonnet (we don't
        // care about it at all since *this* .drone.jsonnet is already loaded).
        'git config merge.ours.driver true',
        'echo .drone.jsonnet merge=ours >>.gitattributes',

        'git merge ${DRONE_COMMIT}',
        'export DEBEMAIL="${DRONE_COMMIT_AUTHOR_EMAIL}" DEBFULLNAME="${DRONE_COMMIT_AUTHOR_NAME}"',
        'gbp dch -S -s "HEAD^" --spawn-editor=never -U low',
        'eatmydata mk-build-deps --install --remove --tool "' + apt_get_quiet + ' -o Debug::pkgProblemResolver=yes --no-install-recommends -y"',
        'export DEB_BUILD_OPTIONS="parallel=$$(nproc)"',
        //'grep -q lib debian/lokinet-bin.install || echo "/usr/lib/lib*.so*" >>debian/lokinet-bin.install',
        'debuild -e CCACHE_DIR -b',
        './contrib/ci/drone-debs-upload.sh ' + distro,
      ],
    },
  ],
};

local clang(version) = debian_pipeline(
  'Debian sid/clang-' + version + ' (amd64)',
  docker_base + 'debian-sid-clang',
  deps=['clang-' + version] + default_deps_nocxx,
  cmake_extra='-DCMAKE_C_COMPILER=clang-' + version + ' -DCMAKE_CXX_COMPILER=clang++-' + version + ' '
);

local full_llvm(version) = debian_pipeline(
  'Debian sid/llvm-' + version + ' (amd64)',
  docker_base + 'debian-sid-clang',
  deps=['clang-' + version, ' lld-' + version, ' libc++-' + version + '-dev', 'libc++abi-' + version + '-dev']
       + default_deps_nocxx,
  cmake_extra='-DCMAKE_C_COMPILER=clang-' + version +
              ' -DCMAKE_CXX_COMPILER=clang++-' + version +
              ' -DCMAKE_CXX_FLAGS=-stdlib=libc++ ' +
              std.join(' ', [
                '-DCMAKE_' + type + '_LINKER_FLAGS=-fuse-ld=lld-' + version
                for type in ['EXE', 'MODULE', 'SHARED']
              ])
);

// Macos build
local mac_builder(name,
                  build_type='Release',
                  werror=true,
                  cmake_extra='',
                  local_mirror=true,
                  extra_cmds=[],
                  jobs=6,
                  codesign='-DCODESIGN=OFF',
                  allow_fail=false) = {
  kind: 'pipeline',
  type: 'exec',
  name: name,
  platform: { os: 'darwin', arch: 'amd64' },
  steps: [
    { name: 'submodules', commands: submodule_commands },
    {
      name: 'build',
      environment: { SSH_KEY: { from_secret: 'SSH_KEY' } },
      commands: [
        'echo "Building on ${DRONE_STAGE_MACHINE}"',
        // If you don't do this then the C compiler doesn't have an include path containing
        // basic system headers.  WTF apple:
        'export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"',
        'ulimit -n 1024',  // because macos sets ulimit to 256 for some reason yeah idk
        './contrib/mac-configure.sh ' +
        ci_dep_mirror(local_mirror) +
        (if build_type == 'Debug' then ' -DWARN_DEPRECATED=OFF ' else '') +
        codesign,
        'cd build-mac',
        // We can't use the 'package' target here because making a .dmg requires an active logged in
        // macos gui to invoke Finder to invoke the partitioning tool to create a partitioned (!)
        // disk image.  Most likely the GUI is required because if you lose sight of how pretty the
        // surface of macOS is you might see how ugly the insides are.
        'ninja -j' + jobs + ' assemble_gui',
        'cd ..',
      ] + extra_cmds,
    },
  ],
};

// iphone builder
local iphone_builder(name,
                     build_type='Release',
                     werror=true,
                     cmake_extra='',
                     local_mirror=true,
                     extra_cmds=[],
                     jobs=6,
                     allow_fail=false) = {
  kind: 'pipeline',
  type: 'exec',
  name: name,
  platform: { os: 'darwin', arch: 'amd64' },
  steps: [
    { name: 'submodules', commands: submodule_commands },
    {
      name: 'build',
      environment: { SSH_KEY: { from_secret: 'SSH_KEY' } },
      commands: [
        'echo "Building on ${DRONE_STAGE_MACHINE}"',
        'export CMAKE_BUILD_PARALLEL_LEVEL=6',
        'ulimit -n 1024',  // because macos sets ulimit to 256 for some reason yeah idk
        './contrib/ios.sh ' + ci_dep_mirror(local_mirror),
      ] + extra_cmds,
    },
  ],
};

local docs_pipeline(name, image, extra_cmds=[], allow_fail=false) = {
  kind: 'pipeline',
  type: 'docker',
  name: name,
  platform: { arch: 'amd64' },
  trigger: { branch: { exclude: ['debian/*', 'ubuntu/*'] } },
  steps: [
    submodules,
    {
      name: 'build',
      image: image,
      pull: 'always',
      [if allow_fail then 'failure']: 'ignore',
      environment: { SSH_KEY: { from_secret: 'SSH_KEY' } },
      commands: [
        'cmake -S . -B build-docs',
        'make -C build-docs doc',
      ] + extra_cmds,
    },
  ],
};


[
  {
    name: 'lint check',
    kind: 'pipeline',
    type: 'docker',
    steps: [{
      name: 'build',
      image: docker_base + 'lint',
      pull: 'always',
      commands: [
        'echo "Building on ${DRONE_STAGE_MACHINE}"',
        apt_get_quiet + ' update',
        apt_get_quiet + ' install -y eatmydata',
        'eatmydata ' + apt_get_quiet + ' install --no-install-recommends -y git clang-format-14 jsonnet',
        './contrib/ci/drone-format-verify.sh',
      ],
    }],
  },
  // documentation builder
  docs_pipeline('Documentation',
                docker_base + 'docbuilder',
                extra_cmds=['UPLOAD_OS=docs ./contrib/ci/drone-static-upload.sh']),

  // Various debian builds
  debian_pipeline('Debian sid (amd64)', docker_base + 'debian-sid'),
  debian_pipeline('Debian sid/Debug (amd64)', docker_base + 'debian-sid', build_type='Debug'),
  clang(13),
  full_llvm(13),
  debian_pipeline('Debian stable (i386)', docker_base + 'debian-stable/i386'),
  debian_pipeline('Debian buster (amd64)', docker_base + 'debian-buster', cmake_extra='-DDOWNLOAD_SODIUM=ON'),
  debian_pipeline('Ubuntu latest (amd64)', docker_base + 'ubuntu-rolling'),
  debian_pipeline('Ubuntu LTS (amd64)', docker_base + 'ubuntu-lts'),
  debian_pipeline('Ubuntu bionic (amd64)',
                  docker_base + 'ubuntu-bionic',
                  deps=['g++-8'] + default_deps_nocxx,
                  cmake_extra='-DCMAKE_C_COMPILER=gcc-8 -DCMAKE_CXX_COMPILER=g++-8',
                  oxen_repo=true),

  // ARM builds (ARM64 and armhf)
  debian_pipeline('Debian sid (ARM64)', docker_base + 'debian-sid', arch='arm64', jobs=4),
  debian_pipeline('Debian stable (armhf)', docker_base + 'debian-stable/arm32v7', arch='arm64', jobs=4),

  // cross compile targets
  linux_cross_pipeline('Cross Compile (arm/arm64)', cross_targets=['arm-linux-gnueabihf', 'aarch64-linux-gnu']),
  linux_cross_pipeline('Cross Compile (ppc64le)', cross_targets=['powerpc64le-linux-gnu']),
  // Not currently building successfully:
  //linux_cross_pipeline('Cross Compile (mips)', cross_targets=['mips-linux-gnu', 'mipsel-linux-gnu']),

  // android apk builder
  apk_builder('android apk', docker_base + 'flutter', extra_cmds=['UPLOAD_OS=android ./contrib/ci/drone-static-upload.sh']),

  // Windows builds (x64)
  windows_cross_pipeline('Windows (amd64)',
                         docker_base + 'debian-bookworm',
                         extra_cmds=[
                           './contrib/ci/drone-static-upload.sh',
                         ]),

  // Static build (on bionic) which gets uploaded to builds.lokinet.dev:
  debian_pipeline('Static (bionic amd64)',
                  docker_base + 'ubuntu-bionic',
                  deps=['g++-8', 'python3-dev', 'automake', 'libtool'],
                  lto=true,
                  tests=false,
                  oxen_repo=true,
                  cmake_extra='-DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON ' +
                              '-DCMAKE_C_COMPILER=gcc-8 -DCMAKE_CXX_COMPILER=g++-8 ' +
                              '-DCMAKE_CXX_FLAGS="-march=x86-64 -mtune=haswell" ' +
                              '-DCMAKE_C_FLAGS="-march=x86-64 -mtune=haswell" ' +
                              '-DNATIVE_BUILD=OFF -DWITH_SYSTEMD=OFF -DWITH_BOOTSTRAP=OFF -DBUILD_LIBLOKINET=OFF',
                  extra_cmds=[
                    './contrib/ci/drone-check-static-libs.sh',
                    './contrib/ci/drone-static-upload.sh',
                  ]),
  // Static armhf build (gets uploaded)
  debian_pipeline('Static (buster armhf)',
                  docker_base + 'debian-buster/arm32v7',
                  arch='arm64',
                  deps=['g++', 'python3-dev', 'automake', 'libtool'],
                  cmake_extra='-DBUILD_STATIC_DEPS=ON -DBUILD_SHARED_LIBS=OFF -DSTATIC_LINK=ON ' +
                              '-DCMAKE_CXX_FLAGS="-march=armv7-a+fp -Wno-psabi" -DCMAKE_C_FLAGS="-march=armv7-a+fp" ' +
                              '-DNATIVE_BUILD=OFF -DWITH_SYSTEMD=OFF -DWITH_BOOTSTRAP=OFF',
                  extra_cmds=[
                    './contrib/ci/drone-check-static-libs.sh',
                    'UPLOAD_OS=linux-armhf ./contrib/ci/drone-static-upload.sh',
                  ],
                  jobs=4),

  // integration tests
  debian_pipeline('Router Hive',
                  docker_base + 'ubuntu-lts',
                  deps=['python3-dev', 'python3-pytest', 'python3-pybind11'] + default_deps,
                  cmake_extra='-DWITH_HIVE=ON'),

  // Deb builds:
  deb_builder(docker_base + 'debian-sid-builder', 'sid', 'debian/sid'),
  deb_builder(docker_base + 'debian-bullseye-builder', 'bullseye', 'debian/bullseye'),
  deb_builder(docker_base + 'ubuntu-jammy-builder', 'jammy', 'ubuntu/jammy'),
  deb_builder(docker_base + 'debian-sid-builder', 'sid', 'debian/sid', arch='arm64'),

  // iphone builds:
  iphone_builder('iOS (embedded lokinet)', build_type='Debug', extra_cmds=[
    'UPLOAD_OS=iphone ./contrib/ci/drone-static-upload.sh',
  ]),

  // Macos builds:
  mac_builder('macOS (Release)', extra_cmds=[
    './contrib/ci/drone-check-static-libs.sh',
    './contrib/ci/drone-static-upload.sh',
  ]),
  mac_builder('macOS (Debug)', build_type='Debug'),
]
