#!/usr/bin/env bash
# Copyright (c) 2014-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C
if ! [[ "$2" =~ ^(git@)?(www.)?github.com(:|/)gobytecoin/gobyte(.git)?$ ]]; then
    exit 0
fi

while read -r local_ref local_oid remote_ref remote_oid; do
    if [ "$remote_ref" != "refs/heads/master" ]; then
        continue
    fi
    if ! ./contrib/verify-commits/verify-commits.py "$local_oid" > /dev/null 2>&1; then
        echo "ERROR: A commit is not signed, can't push"
        ./contrib/verify-commits/verify-commits.py "$local_oid"
        exit 1
    fi
done < /dev/stdin
