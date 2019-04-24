FROM ubuntu:latest

RUN apt update && \
    apt install -y build-essential cmake git libcap-dev curl ninja-build

WORKDIR /src/
COPY . /src/

RUN make NINJA=ninja
#RUN ./lokinet -r -f
COPY lokinet-docker.ini /root/.lokinet/lokinet.ini
RUN ./lokinet-bootstrap

CMD ["./lokinet"]
EXPOSE 1090/udp 1190/tcp
