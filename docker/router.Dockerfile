ARG bootstrap="https://i2p.rocks/i2procks.signed"
FROM alpine:edge as builder

RUN apk update && \
    apk add build-base cmake git libcap-dev libcap-static libuv-dev libuv-static curl ninja bash binutils-gold curl-dev

WORKDIR /src/
COPY . /src/

RUN make NINJA=ninja STATIC_LINK=ON BUILD_TYPE=Release
RUN ./lokinet-bootstrap ${bootstrap}

FROM alpine:latest

COPY lokinet-docker.ini /root/.lokinet/lokinet.ini
COPY --from=builder /src/build/daemon/lokinet .
COPY --from=builder /root/.lokinet/bootstrap.signed /root/.lokinet/

CMD ["./lokinet"]
EXPOSE 1090/udp 1190/tcp
