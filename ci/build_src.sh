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

unset CC; unset CXX
unset DISPLAY

export CCACHE_COMPRESS=${CCACHE_COMPRESS:-1}
export CCACHE_SIZE=${CCACHE_SIZE:-400M}

if [ "$PULL_REQUEST" != "false" ]; then contrib/devtools/commit-script-check.sh "$COMMIT_RANGE"; fi

#if [ "$CHECK_DOC" = 1 ]; then contrib/devtools/check-doc.py; fi TODO reenable after all Bitcoin PRs have been merged and docs fully fixed
if [ "$CHECK_DOC" = 1 ]; then contrib/devtools/check-rpc-mappings.py .; fi
if [ "$CHECK_DOC" = 1 ]; then contrib/devtools/lint-all.sh; fi

ccache --max-size="$CCACHE_SIZE"

if [ -n "$USE_SHELL" ]; then
  export CONFIG_SHELL="$USE_SHELL"
fi

BITCOIN_CONFIG_ALL="--disable-dependency-tracking --prefix=$BUILD_DIR/depends/$HOST --bindir=$OUT_DIR/bin --libdir=$OUT_DIR/lib"

if [ -n "$USE_SHELL" ]; then
  eval '"$USE_SHELL" -c "./autogen.sh"'
else
  ./autogen.sh
fi

rm -rf build-ci
mkdir build-ci
cd build-ci

../configure --cache-file=config.cache "$BITCOIN_CONFIG_ALL" "$BITCOIN_CONFIG" || ( cat config.log && false)
make distdir VERSION="$BUILD_TARGET"

cd gobytecore-"$BUILD_TARGET"
./configure --cache-file=../config.cache "$BITCOIN_CONFIG_ALL" "$BITCOIN_CONFIG" || ( cat config.log && false)

make "$MAKEJOBS" "$GOAL" || ( echo "Build failure. Verbose build follows." && make "$GOAL" V=1 ; false )
