FROM debian:testing
RUN /bin/bash -c 'echo "man-db man-db/auto-update boolean false" | debconf-set-selections'
RUN /bin/bash -c 'sed -i "s/main/main contrib/g" /etc/apt/sources.list'
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q update && apt-get -o=Dpkg::Use-Pty=0 -q dist-upgrade -y && apt-get -o=Dpkg::Use-Pty=0 -q install -y android-sdk google-android-ndk-installer'
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q -y install wget git pkg-config'
RUN /bin/bash -c 'wget https://services.gradle.org/distributions/gradle-6.3-bin.zip -O /tmp/gradle.zip && unzip -d /opt/ /tmp/gradle.zip && ln -s /opt/gradle*/bin/gradle /usr/local/bin/gradle && rm -f /tmp/gradle.zip'
RUN /bin/bash -c 'git clone https://github.com/Shadowstyler/android-sdk-licenses.git /tmp/android-sdk-licenses && cp -a /tmp/android-sdk-licenses/*-license /usr/lib/android-sdk/licenses && rm -rf /tmp/android-sdk-licenses'
RUN /bin/bash -c 'apt-get -o=Dpkg::Use-Pty=0 -q -y install automake libtool'