#!/usr/bin/env bash
# Copyright (c) 2021-2025 The Dash Core developers
# Copyright (c) 2025-2026 The GoByte Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# This script is executed inside the builder image

export LC_ALL=C.UTF-8
set -e

source ./ci/matrix.sh

if [ "$RUN_UNITTESTS" != "true" ]; then
  echo "Skipping unit tests"
  exit 0
fi

# Default seed of 1 is deterministic and always valid for Boost.Test.
# Can be overridden by the caller: BOOST_TEST_RANDOM=<seed> ./ci/test_unittests.sh
export BOOST_TEST_RANDOM="${BOOST_TEST_RANDOM:-1}"
export LD_LIBRARY_PATH="$BUILD_DIR/depends/$HOST/lib"

export WINEDEBUG=fixme-all
export BOOST_TEST_LOG_LEVEL=test_suite

cd "build-ci/gobytecore-$BUILD_TARGET"

# WINEPREFIX must point to a directory writable by the runner uid.
# /tmp is world-writable by POSIX guarantee, independent of Docker
# bind-mount ownership — avoids Wine aborting with "not owned by you"
# when HOME resolves to '/' under rootless Docker.
export WINEPREFIX="/tmp/wine"
mkdir -p "$WINEPREFIX"

if [ "$DIRECT_WINE_EXEC_TESTS" = "true" ]; then
  # Inside Docker, binfmt isn't working so we can't trust in make
  # invoking windows binaries correctly.
  wine ./src/test/test_gobyte.exe
else
  make "$MAKEJOBS" check VERBOSE=1
fi
