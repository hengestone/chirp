#!/bin/sh

set -e

cd /etc/apk
ln -s /outside/ci/alpine/.cache cache
cat > /etc/apk/repositories <<EOF
http://dl-cdn.alpinelinux.org/alpine/v3.6/main
http://dl-cdn.alpinelinux.org/alpine/v3.6/community
@edge http://dl-4.alpinelinux.org/alpine/edge/main
@comm http://dl-4.alpinelinux.org/alpine/edge/community
@test http://dl-4.alpinelinux.org/alpine/edge/testing
EOF

if [ "$TLS" = "openssl" ]; then
    export ITLS=openssl-dev
else
    export ITLS=libressl-dev
fi
apk update
apk upgrade
apk add --no-progress \
    sudo \
    sed \
    alpine-sdk \
    python3 \
    graphviz \
    cppcheck \
    libuv-dev \
    abi-compliance-checker@edge \
    valgrind \
    clang \
    $ITLS
pip3 install \
    pytest \
    hypothesis \
    u-msgpack-python \
    sphinx \
    sphinx_rtd_theme
if [ "$TESTSHELL" = "True" ]; then
    apk add --no-progress gdb
fi
