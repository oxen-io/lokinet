ARG LOKINET_NETID=docker

FROM alpine:edge as builder

RUN apk update && \
    apk add build-base cmake git libcap-dev libcap-static libuv-dev libuv-static curl ninja bash binutils-gold

WORKDIR /src/
COPY . /src/

RUN make NINJA=ninja STATIC_LINK=ON BUILD_TYPE=Release

FROM alpine:latest

COPY ./docker/compose/bootstrap.ini /root/.lokinet/lokinet.ini
COPY --from=builder /src/build/lokinet .

CMD ["./lokinet"]
EXPOSE 1090/udp 1190/tcp
