FROM compose-base:latest

ENV LOKINET_NETID=docker

COPY ./docker/compose/bootstrap.ini /root/.lokinet/lokinet.ini

CMD ["/lokinet"]
EXPOSE 1090/udp 1190/tcp
