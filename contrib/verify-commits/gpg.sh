#!/bin/sh
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C
INPUT=$(cat /dev/stdin)
VALID=false
REVSIG=false
IFS='
'
if [ "$BITCOIN_VERIFY_COMMITS_ALLOW_SHA1" = 1 ]; then
	GPG_RES="$(printf '%s\n' "$INPUT" | gpg --trust-model always "$@" 2>/dev/null)"
else
	GPG_RES=""
	for LINE in $(gpg --version); do
		case "$LINE" in
			"gpg (GnuPG) 1.4.1"*|"gpg (GnuPG) 2.0."*)
				echo "Please upgrade to at least gpg 2.1.10 to check for weak signatures" > /dev/stderr
				GPG_RES="$(printf '%s\n' "$INPUT" | gpg --trust-model always "$@" 2>/dev/null)"
				;;
		esac
	done
	[ "$GPG_RES" = "" ] && GPG_RES="$(printf '%s\n' "$INPUT" | gpg --trust-model always --weak-digest sha1 "$@" 2>/dev/null)"
fi
for LINE in $GPG_RES; do
	case "$LINE" in
	"[GNUPG:] VALIDSIG "*)
		while read -r KEY; do
			[ "${LINE#?GNUPG:? VALIDSIG * * * * * * * * * * }" = "$KEY" ] && VALID=true
		done < ./contrib/verify-commits/trusted-keys
		;;
	"[GNUPG:] REVKEYSIG "*)
		[ "$BITCOIN_VERIFY_COMMITS_ALLOW_REVSIG" != 1 ] && exit 1
		REVSIG=true
		GOODREVSIG="[GNUPG:] GOODSIG ${LINE#* * *}"
		;;
	"[GNUPG:] EXPKEYSIG "*)
		[ "$BITCOIN_VERIFY_COMMITS_ALLOW_REVSIG" != 1 ] && exit 1
		REVSIG=true
		GOODREVSIG="[GNUPG:] GOODSIG ${LINE#* *}"
		;;
	esac
done
if ! $VALID; then
	exit 1
fi
if $VALID && $REVSIG; then
	printf '%s\n' "$INPUT" | gpg --trust-model always "$@" 2>/dev/null | grep "^\[GNUPG:\] \(NEWSIG\|SIG_ID\|VALIDSIG\)"
	echo "$GOODREVSIG"
else
	printf '%s\n' "$INPUT" | gpg --trust-model always "$@" 2>/dev/null
fi
for LINE in $GPG_RES; do
	case "$LINE" in
	"[GNUPG:] VALIDSIG "*)
		while read -r KEY; do
			[ "${LINE#?GNUPG:? VALIDSIG * * * * * * * * * }" = "$KEY" ] && VALID=true
		done < ./contrib/verify-commits/trusted-keys
		;;
	"[GNUPG:] REVKEYSIG "*)
		[ "$BITCOIN_VERIFY_COMMITS_ALLOW_REVSIG" != 1 ] && exit 1
		REVSIG=true
		GOODREVSIG="[GNUPG:] GOODSIG ${LINE#* * *}"
		;;
	"[GNUPG:] EXPKEYSIG "*)
		[ "$BITCOIN_VERIFY_COMMITS_ALLOW_REVSIG" != 1 ] && exit 1
		REVSIG=true
		GOODREVSIG="[GNUPG:] GOODSIG ${LINE#* * *}"
		;;
	esac
done
if ! $VALID; then
	exit 1
fi
if $VALID && $REVSIG; then
	printf '%s\n' "$INPUT" | gpg --trust-model always "$@" 2>/dev/null | grep "^\[GNUPG:\] \(NEWSIG\|SIG_ID\|VALIDSIG\)"
	echo "$GOODREVSIG"
else
	printf '%s\n' "$INPUT" | gpg --trust-model always "$@" 2>/dev/null
fi
