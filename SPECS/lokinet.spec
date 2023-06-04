Name:           lokinet
Version:        0.9.11
Release:        1%{?dist}
Summary:        Lokinet anonymous, decentralized overlay network

License:        GPLv3+
URL:            https://lokinet.org
Source0:        %{name}-%{version}.src.tar.gz

%global sonamever %{version}

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig
BuildRequires:  libuv-devel
BuildRequires:  oxenmq-devel
BuildRequires:  unbound-devel
BuildRequires:  libsodium-devel
BuildRequires:  systemd-devel
BuildRequires:  systemd-rpm-macros
BuildRequires:  libcurl-devel
BuildRequires:  jemalloc-devel
BuildRequires:  sqlite3-devel

Requires: lokinet-bin = %{version}-%{release}
%{?systemd_requires}

%description

Lokinet private, decentralized, market-based, Sybil resistant overlay network
for the internet.  Lokinet uses Oxen Service Nodes as relays to provide
censorship resistance and privacy for communication between Lokinet network
clients.

This package contains the lokinet configuration and dependencies needed to
connect to lokinet as a client or SNapp.

%package bin
Summary: Lokinet anonymous, decentralized overlay network -- binaries

%description bin

This package contains the common binaries for lokinet packages.  Most users will
want to install the lokinet package rather than this one to run lokinet as a
system service.

%package monitor
Summary: lokinetmon monitoring tool for lokinet
Requires: python3
Requires: python3-zmq
Recommends: lokinet

%description monitor

This package contains the lokinetmon command-line tool for advanced monitoring
of a running lokinet instance.

%prep

%autosetup -p1

%build

%define cmake_extra_args %{nil}
%ifarch x86_64
export CXXFLAGS="%{optflags} -mtune=haswell"
export CFLAGS="%{optflags} -mtune=haswell"
%endif
%ifarch aarch64
%define cmake_extra_args -DNON_PC_TARGET=ON
export CXXFLAGS="%{optflags} -march=armv8-a+crc -mtune=cortex-a72"
export CFLAGS="%{optflags} -march=armv8-a+crc -mtune=cortex-a72"
%endif
%ifarch %{arm}
%define cmake_extra_args -DNON_PC_TARGET=ON
export CXXFLAGS="%{optflags} -march=armv6 -mtune=cortex-a53 -mfloat-abi=hard -mfpu=vfp"
export CFLAGS="%{optflags} -march=armv6 -mtune=cortex-a53 -mfloat-abi=hard -mfpu=vfp"
%endif

%undefine __cmake_in_source_build
%cmake -DNATIVE_BUILD=OFF -DUSE_AVX2=OFF -DWITH_TESTS=OFF %{cmake_extra_args} -DCMAKE_BUILD_TYPE=Release -DLOKINET_VERSIONTAG="%{release}" -DWITH_SETCAP=OFF -DSUBMODULE_CHECK=OFF -DBUILD_SHARED_LIBS=OFF -DBUILD_LIBLOKINET=OFF
%cmake_build

%install

%cmake_install

install -m755 contrib/py/admin/lokinetmon $RPM_BUILD_ROOT/%{_bindir}/
install -Dm644 SOURCES/lokinet.service $RPM_BUILD_ROOT/%{_unitdir}/lokinet.service
install -Dm644 contrib/systemd-resolved/lokinet.rules $RPM_BUILD_ROOT/%{_datadir}/polkit-1/rules.d/50-lokinet.rules
install -Dm644 SOURCES/dnssec-lokinet.negative $RPM_BUILD_ROOT%{_exec_prefix}/lib/dnssec-trust-anchors.d/lokinet.negative
install -Dm644 SOURCES/bootstrap.signed $RPM_BUILD_ROOT%{_sharedstatedir}/lokinet/bootstrap.signed


%files

%license LICENSE
%doc readme.*
%{_datadir}/polkit-1/rules.d/50-lokinet.rules
%{_unitdir}/lokinet.service

%files bin

%{_bindir}/lokinet
%{_bindir}/lokinet-bootstrap
%{_bindir}/lokinet-vpn
%{_exec_prefix}/lib/dnssec-trust-anchors.d/lokinet.negative
%{_sharedstatedir}/lokinet/bootstrap.signed

%files monitor

%{_bindir}/lokinetmon

%pre bin


# Create the _lokinet/_loki user/group
if ! getent group _loki >/dev/null; then
    groupadd --system _loki
fi
if ! getent passwd _lokinet >/dev/null; then
    useradd --badname --system --home-dir /var/lib/lokinet --group _loki --comment "Lokinet system user" _lokinet
fi

# Make sure the _lokinet user is part of the _loki group (in case it already existed)
if ! id -Gn _lokinet | grep -qw _loki; then
    usermod _lokinet -g _loki
fi

%post

datadir=/var/lib/lokinet
mkdir -p $datadir
chown _lokinet:_loki $datadir

if ! [ -e /etc/loki/lokinet.ini ]; then
    mkdir -p /etc/loki
    /usr/bin/lokinet -g /etc/loki/lokinet.ini
    chmod 640 /etc/loki/lokinet.ini
    chown _lokinet:_loki /etc/loki/lokinet.ini
    ln -sf /etc/loki/lokinet.ini /var/lib/lokinet/lokinet.ini
fi

%systemd_post lokinet.service
systemctl enable --now lokinet

%preun
%systemd_preun lokinet.service

%postun
%systemd_postun lokinet.service

%changelog
* Sat Jun 03 2023 Jason Rhinelander <jason@imaginary.ca> - 0.9.11-1
- bump version
- fix use of `--badnames` instead of `--badname`
- drop version tag patch; there is a cmake option for that now

* Wed Nov 17 2021 Technical Tumbleweed <necro_nemesis@hotmail.com> - 0.9.8-1
- bump version

* Wed Oct 20 2021 Technical Tumbleweed <necro_nemesis@hotmail.com> - 0.9.7-1
- bump version

* Thu Oct 14 2021 Technical Tumbleweed <necro_nemesis@hotmail.com> - 0.9.6~1
- Remove Patch2: default-dns.patch

* Thu Sep 09 2021 Jason Rhinelander <jason@imaginary.ca> - 0.9.6-1
- 0.9.6 release.
- bundle bootstrap.signed instead of downloading
- drop default upstream dns patch (fixed in 0.9.6)
- Clone/update fedora/34 build for opensuse 15.3

* Thu Aug 12 2021 Jason Rhinelander <jason@imaginary.ca> - 0.9.5-6
- Change default dns port from 1053 to 953 so that it is still privileged.

* Wed Aug 11 2021 Jason Rhinelander <jason@imaginary.ca> - 0.9.5-5
- Apply default upstream dns patch from PR #1715

* Wed Aug 11 2021 Jason Rhinelander <jason@imaginary.ca> - 0.9.5-4
- Change default DNS address to 127.0.0.1:1053 because systemd-resolved has trouble with 127.3.2.1
  for unknown reasons.
- Make it work

* Tue Aug 10 2021 Jason Rhinelander <jason@imaginary.ca> - 0.9.5-3
- Updated for rpm.oxen.io packaging
- Split into lokinet/lokinet-bin/lokinet-monitor packages

* Thu Jul 22 2021 Technical Tumbleweed <necro_nemesis@hotmail.com> Lokinet 0.9.5
- Build with systemd-resolved and binary lokinet-bootstrap

* Sun Mar 07 2021 Technical Tumbleweed <necro_nemesis@hotmail.com> Lokinet 0.8.2
- First Lokinet RPM
