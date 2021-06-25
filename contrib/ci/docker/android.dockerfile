FROM debian:testing
RUN /bin/bash -c 'echo "man-db man-db/auto-update boolean false" | debconf-set-selections'
RUN /bin/bash -c 'sed -i "s/main/main contrib/g" /etc/apt/sources.list'
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q update && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y && apt-get -o=Dpkg::Use-Pty=0 -q install -y android-sdk google-android-ndk-installer wget git pkg-config automake libtool cmake ccache curl zip'
RUN /bin/bash -c 'git clone https://github.com/Shadowstyler/android-sdk-licenses.git /tmp/android-sdk-licenses && cp -a /tmp/android-sdk-licenses/*-license /usr/lib/android-sdk/licenses && rm -rf /tmp/android-sdk-licenses'
RUN /bin/bash -c 'wget https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_2.2.2-stable.tar.xz -O /tmp/flutter.tar.xz && cd /opt && tar -xJvf /tmp/flutter.tar.xz && rm /tmp/flutter.tar.xz && ln -s /opt/flutter/bin/flutter /usr/local/bin/'
RUN /bin/bash -c 'flutter precache'