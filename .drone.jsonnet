local distro = "fedora-36";
local distro_name = 'Fedora 36';
local distro_docker = 'fedora:36';

local submodules = {
    name: 'submodules',
    image: 'drone/git',
    commands: ['git fetch --tags', 'git submodule update --init --recursive --depth=1']
};

local dnf(arch) = 'dnf -y --setopt install_weak_deps=False --setopt cachedir=/cache/'+distro+'/'+arch+'/${DRONE_STAGE_MACHINE} ';

local rpm_pipeline(image, buildarch='amd64', rpmarch='x86_64', jobs=6) = {
    kind: 'pipeline',
    type: 'docker',
    name: distro_name + ' (' + rpmarch + ')',
    platform: { arch: buildarch },
    steps: [
        submodules,
        {
            name: 'build',
            image: image,
            environment: {
                SSH_KEY: { from_secret: "SSH_KEY" },
                RPM_BUILD_NCPUS: jobs
            },
            commands: [
                'echo "Building on ${DRONE_STAGE_MACHINE}"',
                dnf(rpmarch) + 'distro-sync',
                dnf(rpmarch) + 'install rpm-build git-archive-all dnf-plugins-core ccache',
                dnf(rpmarch) + 'config-manager --add-repo https://rpm.oxen.io/fedora/oxen.repo',
                'pkg_src_base="$(rpm -q --queryformat=\'%{NAME}-%{VERSION}\n\' --specfile SPECS/lokinet.spec | head -n 1)"',
                'git-archive-all --prefix $pkg_src_base/ SOURCES/$pkg_src_base.src.tar.gz',
                dnf(rpmarch) + 'builddep --spec SPECS/lokinet.spec',
                'if [ -n "$CCACHE_DIR" ]; then mkdir -pv ~/.cache; ln -sv "$CCACHE_DIR" ~/.cache/ccache; fi',
                'rpmbuild --define "_topdir $(pwd)" -bb SPECS/lokinet.spec',
                './SPECS/ci-upload.sh ' + distro + ' ' + rpmarch,
            ],
        }
    ]
};

[
     rpm_pipeline(distro_docker),
     rpm_pipeline("arm64v8/" + distro_docker, buildarch='arm64', rpmarch="aarch64", jobs=3)
]
