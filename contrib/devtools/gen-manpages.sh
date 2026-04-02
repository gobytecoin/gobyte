#!/usr/bin/env bash

export LC_ALL=C
TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
BUILDDIR=${BUILDDIR:-$TOPDIR}

BINDIR=${BINDIR:-$BUILDDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

BITCOIND=${BITCOIND:-$BINDIR/gobyted}
BITCOINCLI=${BITCOINCLI:-$BINDIR/gobyte-cli}
BITCOINTX=${BITCOINTX:-$BINDIR/gobyte-tx}
BITCOINQT=${BITCOINQT:-$BINDIR/qt/gobyte-qt}

[ ! -x "$BITCOIND" ] && echo "$BITCOIND not found or not executable." && exit 1

IFS=' ' read -ra BTCVER <<< "$($BITCOINCLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }')"

echo "[COPYRIGHT]" > footer.h2m
$BITCOIND --version | sed -n '1!p' >> footer.h2m

for cmd in "$BITCOIND" "$BITCOINCLI" "$BITCOINTX" "$BITCOINQT"; do
  cmdname="${cmd##*/}"
  help2man -N --version-string="${BTCVER[0]}" --include=footer.h2m -o "${MANDIR}"/"${cmdname}".1 "${cmd}"
  sed -i "s/\\\-${BTCVER[1]}//g" "${MANDIR}"/"${cmdname}".1
done

rm -f footer.h2m
