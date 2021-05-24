#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/..

DOCKER_IMAGE=${DOCKER_IMAGE:-gobytecoin/gobyted-develop}
DOCKER_TAG=${DOCKER_TAG:-latest}

BUILD_DIR=${BUILD_DIR:-.}

rm docker/bin/*
mkdir docker/bin
cp $BUILD_DIR/src/gobyted docker/bin/
cp $BUILD_DIR/src/gobyte-cli docker/bin/
cp $BUILD_DIR/src/gobyte-tx docker/bin/
strip docker/bin/gobyted
strip docker/bin/gobyte-cli
strip docker/bin/gobyte-tx

docker build --pull -t $DOCKER_IMAGE:$DOCKER_TAG -f docker/Dockerfile docker
