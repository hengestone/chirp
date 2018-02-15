#!/bin/sh

curl -s https://www.adfinis-sygroup.ch/adsy-logo.txt
set -e

SCRIPT="$(pwd -P)/$0"
BASE="${SCRIPT%/*}"
echo Running alpine docker test at $BASE
cd "$BASE"

if [ -z "$CC" ]; then
    echo Set CC=gcc since CC is undefined
    export CC=gcc
fi
if [ -z "$TLS" ]; then
    echo Set TLS=libressl since TLS is undefined
    export TLS=libressl
fi

if [ "$1" = "shell" ]; then
    export TESTSHELL=True
else
    export TESTSHELL=False
fi

sudo docker run -it \
    -e "HUID=$(id -u)" \
    -e "CC=$CC" \
    -e "TLS=$TLS" \
    -e "MODE=$MODE" \
    -e "TESTSHELL=$TESTSHELL" \
    -e "VERBOSE=$VERBOSE" \
    -e "DOC_FORMAT=$DOC_FORMAT" \
    -e "IS_ALPINE_CI=True" \
    -v "$(pwd -P)/..":/outside \
    --rm \
    alpine:3.7 \
    /outside/ci/alpine/run.sh
