FROM debian:stretch
LABEL maintainer="GoByte Developers <dev@gobyte.network>"
LABEL description="Dockerised GoByteCore, built from Travis"

RUN apt-get update && apt-get -y upgrade && apt-get clean && rm -fr /var/cache/apt/*

COPY bin/* /usr/bin/
