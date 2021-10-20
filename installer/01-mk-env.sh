#!/usr/bin/env bash

source include/functions.env

echo_note "WARNING" "Installing Development Tools"
sudo dnf group install -y -q "Development Tools"
echo_note "OK" "Packages installed!"

echo_note "WARNING" "Installing Dependency packages"
sudo dnf install -y -q \
    audit-libs-devel \
    libuuid-devel \
    isomd5sum-devel \
    glib2-devel \
    NetworkManager-libnm-devel \
    squashfs-tools \
    e2fsprogs-devel \
    popt-devel \
    libblkid-devel \
    libX11-devel \
    libnl3-devel \
    newt-devel \
    device-mapper-devel \
    python \
    python-devel \
    zlib-devel \
    redhat-lsb-core \
    dmraid-devel \
    net-tools \
    nfs-utils \
    strace \
    tree \
    vim \
    gdb \
    grub2-tools \
    parted-devel \
    python3-dialog \
    dhcp-client \
    policycoreutils \
    genisoimage \
    python3-pyparted \
    udisks2 \
    nasm \
    dbus-devel \
    xorriso 

echo_note "OK" "Packages installed!"

if [ ! -d syslinux ]; then
    echo_note "WARNING" "Expanding Syslinux folder"
    tar xzf addons/syslinux-6.04-pre1.tar.gz && mv syslinux-6.04-pre1 syslinux
    pushd syslinux
    for i in $(ls ../000?-*); do cat $i | patch -p1; done
    popd
fi

