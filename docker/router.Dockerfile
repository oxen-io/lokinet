FROM ubuntu:latest

RUN apt update && \
    apt install -y build-essential cmake git libcap-dev curl ninja-build

WORKDIR /src/
COPY . /src/

# 12p/24l cores takes 8gb
ARG BIG_AND_FAST="false"

RUN if [ "false$BIG_AND_FAST" = "false" ] ; then make ; else make NINJA=ninja ; fi
RUN find . -name lokinet
RUN ./lokinet -g -f
RUN ./lokinet-bootstrap http://206.81.100.174/n-st-1.signed

CMD ["./lokinet"]
EXPOSE 1090/udp
