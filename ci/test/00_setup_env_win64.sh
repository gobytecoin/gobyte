#!/usr/bin/env bash
#
# Copyright (c) 2019-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_win64
export HOST=x86_64-w64-mingw32
export DPKG_ADD_ARCH="i386"
export PACKAGES="python3 nsis g++-mingw-w64-x86-64-posix wine-binfmt wine64 wine32 file"
export RUN_FUNCTIONAL_TESTS=false
export GOAL="deploy"
# -Wno-error=maybe-uninitialized: GCC emits false-positive uninitialized
# diagnostics in cross-compilation contexts at -O2; these do not reflect
# real defects. Ref: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80635
#
# -Wno-error=array-bounds: suppress GCC 12+ array-bounds false positives
# in Boost and other vendored headers when targeting mingw32.
# Ref: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105069
export BITCOIN_CONFIG="--enable-gui --enable-reduce-exports --disable-miner CXXFLAGS='-Wno-error=maybe-uninitialized -Wno-error=array-bounds'"
export DIRECT_WINE_EXEC_TESTS=true
