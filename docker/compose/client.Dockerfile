FROM compose-base:latest

COPY ./docker/compose/client.ini /root/.lokinet/lokinet.ini

CMD ["/lokinet"]
EXPOSE 1090/udp 1190/tcp
