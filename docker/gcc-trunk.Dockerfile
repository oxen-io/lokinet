FROM gcc:latest

RUN apt update && \
    apt install -y cmake git libcap-dev curl ninja-build libuv1-dev

WORKDIR /src/

COPY . /src/

RUN mkdir build && \
    cd build && \
    cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DWARNINGS_AS_ERRORS=ON -DDOWNLOAD_SODIUM=ON && \
    ninja -k0 && \
    ./test/testAll
