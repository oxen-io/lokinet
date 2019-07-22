FROM debian:stable

RUN apt update && \
    apt install -y build-essential cmake git libcap-dev curl ninja-build libuv1-dev g++-mingw-w64 gcc-mingw-w64-base g++-mingw-w64-x86-64 mingw-w64 mingw-w64-common

WORKDIR /src/

COPY . /src/

RUN make windows NINJA=ninja
