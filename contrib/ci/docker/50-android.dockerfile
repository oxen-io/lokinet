FROM registry.oxen.rocks/lokinet-ci-debian-testing-base
RUN /bin/bash -c 'sed -i "s/main/main contrib/g" /etc/apt/sources.list'
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q update && apt-get -o=Dpkg::Use-Pty=0 -q install --no-install-recommends -y android-sdk google-android-ndk-installer wget git pkg-config automake libtool cmake ccache curl zip xz-utils openssh-client && git clone https://github.com/Shadowstyler/android-sdk-licenses.git /tmp/android-sdk-licenses && cp -a /tmp/android-sdk-licenses/*-license /usr/lib/android-sdk/licenses && rm -rf /tmp/android-sdk-licenses'
