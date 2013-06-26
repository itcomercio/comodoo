#!/bin/bash

VERSION=3.85
SYSLINUX=syslinux-${VERSION}

[ -a $SYSLINUX.tar.bz2 ] || wget http://www.kernel.org/pub/linux/utils/boot/syslinux/$SYSLINUX.tar.bz2

[ -a syslinux ] || (tar xjf $SYSLINUX.tar.bz2 && mv $SYSLINUX syslinux)

# we are going to use the precompiled

# cd syslinux
# make clean installer


