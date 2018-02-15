#!/bin/sh

set -e

SCRIPT="$(pwd -P)/$0"
BASE="${SCRIPT%/*}"
echo "Running arch docker test at $BASE"
cd "$BASE"

if [ -z "$CC" ]; then
    echo Set CC=gcc since CC is undefined
    export CC=gcc
fi

if [ "$1" = "shell" ]; then
    export TESTSHELL=True
else
    export TESTSHELL=False
fi

sudo docker run -it \
    -e "HUID=$(id -u)" \
    -e "CC=$CC" \
    -e "TESTSHELL=$TESTSHELL" \
    -e "NO_MEMCHECK=$NO_MEMCHECK" \
    -v "$(pwd -P)/..":/outside \
    -v "$(pwd -P)/arch/.cache":/var/cache/pacman/pkg \
    --rm \
    base/archlinux:latest \
    /outside/ci/arch/run.sh
