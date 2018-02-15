#!/bin/sh

set -e

pacman -Syu --noconfirm --noprogressbar 2> /dev/null
pacman -S --noconfirm --noprogressbar \
    sudo \
    base-devel \
    python-pip \
    python-sphinx \
    python-sphinx_rtd_theme \
    graphviz \
    cppcheck \
    openssl \
    libuv \
    clang
if [ "$NO_MEMCHECK" != "True" ]; then
    pacman -S --noconfirm --noprogressbar valgrind
fi
if [ "$TESTSHELL" = "True" ]; then
    pacman -S --noconfirm --noprogressbar gdb 2> /dev/null
fi
pip3 install pytest hypothesis u-msgpack-python
