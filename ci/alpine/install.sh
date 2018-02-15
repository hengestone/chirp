#!/bin/sh

set -e

cd /etc/apk
ln -s /outside/ci/alpine/.cache cache
cat > /etc/apk/repositories <<EOF
http://dl-cdn.alpinelinux.org/alpine/v3.7/main
http://dl-cdn.alpinelinux.org/alpine/v3.7/community
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
    abi-compliance-checker \
    clang \
    $ITLS
if [ "$NO_MEMCHECK" != "True" ]; then
    apk add --no-progress valgrind
fi
pip3 install \
    pytest \
    hypothesis \
    u-msgpack-python \
    sphinx \
    sphinx_rtd_theme
if [ "$TESTSHELL" = "True" ]; then
    apk add --no-progress gdb
fi
