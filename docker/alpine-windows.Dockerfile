FROM alpine:edge as builder

RUN apk update && \
    apk --no-cache add --repository http://dl-cdn.alpinelinux.org/alpine/edge/testing build-base cmake git libcap-dev libcap-static libuv-dev libuv-static curl ninja bash binutils-gold mingw-w64-gcc

WORKDIR /src/
COPY . /src/

RUN make windows-release NINJA=ninja STATIC_LINK=ON DOWNLOAD_SODIUM=ON
