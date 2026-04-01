#!/usr/bin/env bash
# Copyright (c) 2021-2026 The Dash Core developers
# Copyright (c) 2026-2027 The Gobyte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"/../.. || exit

DOCKER_IMAGE=${DOCKER_IMAGE:-gobytecoin/gobyted-develop}
DOCKER_TAG=${DOCKER_TAG:-latest}

if [ -n "$DOCKER_REPO" ]; then
  DOCKER_IMAGE_WITH_REPO=$DOCKER_REPO/$DOCKER_IMAGE
else
  DOCKER_IMAGE_WITH_REPO=$DOCKER_IMAGE
fi

docker tag "$DOCKER_IMAGE":"$DOCKER_TAG" "$DOCKER_IMAGE_WITH_REPO":"$DOCKER_TAG"
docker push "$DOCKER_IMAGE_WITH_REPO":"$DOCKER_TAG"
docker rmi "$DOCKER_IMAGE_WITH_REPO":"$DOCKER_TAG"
