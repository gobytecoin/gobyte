#!/bin/sh

export LC_ALL=C

set -e
# shellcheck disable=SC2086
srcdir="$(dirname $0)"
cd "$srcdir"
if [ -z "${LIBTOOLIZE}" ] && GLIBTOOLIZE="$(which glibtoolize 2>/dev/null)"; then
  LIBTOOLIZE="${GLIBTOOLIZE}"
  export LIBTOOLIZE
fi
autoreconf --install --force
