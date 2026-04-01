FROM phusion/baseimage:jammy-1.0.4
LABEL maintainer="GoByte Developers <dev@gobyte.network>"
LABEL description="Dockerised GoByteCore, built from CI"

ARG USER_ID
ARG GROUP_ID

ENV HOME="/home/gobyte"

# add user with specified (or default) user/group ids
ENV USER_ID="${USER_ID:-1000}"
ENV GROUP_ID="${GROUP_ID:-1000}"
RUN groupadd -g ${GROUP_ID} gobyte && \
    useradd -u ${USER_ID} -g gobyte -s /bin/bash -m -d /home/gobyte gobyte && \
    mkdir /home/gobyte/.gobytecore && \
    chown ${USER_ID}:${GROUP_ID} -R /home/gobyte

COPY bin/* /usr/local/bin/

RUN chmod a+x /usr/local/bin/*

USER gobyte

VOLUME ["/home/gobyte"]

EXPOSE 12454 12455 13454 13455

WORKDIR /home/gobyte
