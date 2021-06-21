FROM debian:testing
RUN /bin/bash -c 'echo "man-db man-db/auto-update boolean false" | debconf-set-selections'
RUN /bin/bash -c 'sed -i "s/main/main contrib/g" /etc/apt/sources.list'
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q update && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y && apt-get -o=Dpkg::Use-Pty=0 -q install -y android-sdk google-android-ndk-installer'
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q -y install wget git pkg-config'
RUN /bin/bash -c 'git clone https://github.com/Shadowstyler/android-sdk-licenses.git /tmp/android-sdk-licenses && cp -a /tmp/android-sdk-licenses/*-license /usr/lib/android-sdk/licenses && rm -rf /tmp/android-sdk-licenses'
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q -y install automake libtool cmake ccache curl'
RUN /bin/bash -c 'wget https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_1.22.6-stable.tar.xz -O /tmp/flutter.tar.xz && cd /opt && tar -xJvf /tmp/flutter.tar.xz && rm /tmp/flutter.tar.xz'
RUN /bin/bash -c '/opt/flutter/bin/flutter precache'