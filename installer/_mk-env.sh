#!/usr/bin/env bash

source include/functions.env

echo_note "WARNING" "Installing Development Tools"
dnf group install -y -q "Development Tools"
echo_note "OK" "Packages installed!"

echo_note "WARNING" "Installing Dependency packages"
dnf install -y -q audit-libs-devel \
isomd5sum-devel \
NetworkManager-devel \
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
gdb \
grub2-tools \
parted-devel \
audit-libs-devel \
e2fsprogs-devel \
popt-devel \
libblkid-devel \
libnl3-devel \
NetworkManager-glib-devel \
device-mapper-devel \
dmraid-devel \
python2-dialog \
dhcp-client \
e2fsprogs \
policycoreutils \
genisoimage \
python2-dbus \
python2-pyparted \
udisks2 \
nasm \
xorriso

echo_note "OK" "Packages installed!"

echo_note "WARNING" "Expanding Syslinux folder"
tar xzf addons/syslinux-6.04-pre1.tar.gz && mv syslinux-6.04-pre1 syslinux

