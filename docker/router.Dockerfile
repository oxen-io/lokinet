FROM alpine:latest as builder

RUN apk update && \
    apk add build-base cmake git libcap-dev libuv-dev curl ninja bash binutils-gold

WORKDIR /src/
COPY . /src/

RUN make NINJA=ninja STATIC_LINK=ON BUILD_TYPE=Release
RUN ./lokinet-bootstrap

FROM alpine:latest

COPY lokinet-docker.ini /root/.lokinet/lokinet.ini
COPY --from=builder /src/build/lokinet .
COPY --from=builder /root/.lokinet/bootstrap.signed /root/.lokinet/

CMD ["./lokinet"]
EXPOSE 1090/udp 1190/tcp
