#!/bin/sh

set -e

mkdir -p /tmp/build
cd /tmp/build
/outside/configure \
    --dev --doc --disable-coverage
if [ "$TESTSHELL" = "True" ]; then
    exec /bin/sh
else
    make test
    make doc
fi
