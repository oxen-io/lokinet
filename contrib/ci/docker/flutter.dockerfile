ARG ARCH=amd64
FROM registry.oxen.rocks/lokinet-ci-android/${ARCH}
RUN cd /opt \
    && curl https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_2.2.2-stable.tar.xz \
        | tar xJv \
    && ln -s /opt/flutter/bin/flutter /usr/local/bin/ \
    && flutter precache
