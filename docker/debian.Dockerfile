FROM debian:latest

RUN apt update && \
    apt install -y build-essential cmake git libcap-dev wget rapidjson-dev

WORKDIR /src/

COPY . /src/

RUN make -j 8 JSONRPC=ON CXX17=OFF
