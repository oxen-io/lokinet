FROM ubuntu:latest

RUN apt update && \
    apt install -y build-essential cmake git libcap-dev curl ninja-build

WORKDIR /src/

COPY . /src/

RUN make NINJA=ninja JSONRPC=ON
