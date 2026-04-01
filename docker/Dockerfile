FROM phusion/baseimage:bionic-1.0.0
LABEL maintainer="GoByte Developers <dev@gobyte.network>"
LABEL description="Dockerised GoByteCore, built from Github Actions CI"

ARG USER_ID
ARG GROUP_ID

ENV HOME /gobyte

# add user with specified (or default) user/group ids
ENV USER_ID ${USER_ID:-1000}
ENV GROUP_ID ${GROUP_ID:-1000}
RUN groupadd -g ${GROUP_ID} gobyte
RUN useradd -u ${USER_ID} -g gobyte -s /bin/bash -m -d /gobyte gobyte
RUN mkdir /gobyte/.gobytecore
RUN chown gobyte:gobyte -R /gobyte

COPY bin/* /usr/local/bin/

RUN chmod a+x /usr/local/bin/*

USER gobyte

VOLUME ["/gobyte"]

EXPOSE 12454 12455 13454 13455

WORKDIR /gobyte
