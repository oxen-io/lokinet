FROM ubuntu:xenial

RUN apt update && \
    apt-get install -y apt-transport-https ca-certificates gnupg software-properties-common wget && \
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | apt-key add - && \
    apt-add-repository 'deb https://apt.kitware.com/ubuntu/ xenial main' && \
    apt-get update && \
    apt install -y build-essential cmake git libcap-dev curl ninja-build libuv1-dev

WORKDIR /src/

COPY . /src/

RUN make NINJA=ninja JSONRPC=ON STATIC_LINK=ON
