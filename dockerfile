FROM ubuntu:20.04
COPY . /DHCPServer
WORKDIR "/DHCPServer"
RUN apt update && apt install -y python-is-python3 \
    nano \
    gcc \
    build-essential
RUN gcc -pthread -o server server.c
EXPOSE 67/udp
CMD [""]