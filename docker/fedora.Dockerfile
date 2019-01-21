FROM fedora:latest

RUN dnf update -y && \
    dnf upgrade -y && \
    dnf install -y cmake make git gcc gcc-c++ libcap-devel curl rapidjson-devel

WORKDIR /src/

COPY . /src/

RUN make -j8 JSONRPC=ON
