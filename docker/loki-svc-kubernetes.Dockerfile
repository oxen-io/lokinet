FROM debian:stable

RUN apt update && \
    apt install -y build-essential cmake git libcap-dev curl python3-dev python3-setuptools libsodium-dev

WORKDIR /src/

COPY . /src/

RUN make -j 8 SHARED_LIB=ON JSONRPC=ON && make kubernetes-install

ENTRYPOINT [ "/usr/bin/python3", "-m", "pylokinet"]
