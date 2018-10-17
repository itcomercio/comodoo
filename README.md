# Yocto BSP installer for hard drive based devices

- Project kick-off

Originally this installer was based on Anaconda installer, 
from Fedora 10 branch.

- Minimal packages in Fedora based distro:

$ sudo dnf group install "Development Tools"

$ sudo dnf install audit-libs-devel \
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
python2-dialog

pip install pyparted

#
# Early phase-0
#

====> 0. The installation process:

POS BIOS/
    |-- isolinux boot monitor
    |-- stage-1: init
    |-- stage-1: loader
    `-- stage-2: a-installer

====> 1. stage-1/init and stage-1/loader are just from anaconda sources

$ make -C isys/ && make -C stage-1/

====> 2. Bootdisk creation: isolinux

bootdisk/
`-- isolinux
    |-- isolinux.cfg
    `-- splash.jpg

$ sh create-bootdisk.sh

bootdisk/
`-- isolinux
    |-- isolinux.bin ==> just from tarball in binary format
    |-- isolinux.cfg
    |-- splash.jpg
    `-- vesamenu.c32 ==> just from tarball in binary format

Note about vesamenu.c32:

com32/modules/vesamenu.c32 (graphical) 
com32/modules/menu.c32 (text only menu)

With vesamenu.c32 we can use a background image normally 640x480
pixels and either in PNG, JPEG or LSS16 format.

We can use the binary files from the native package of development workstation. In
Fedora 10:

$ rpm -ql syslinux | grep -E "isolinux.bin|vesamenu.c32" 
/usr/lib/syslinux/isolinux.bin
/usr/lib/syslinux/vesamenu.c32

====> 3. Bootdisk creation: initrd

First of all, we're going to use the native binaries of the development
workstation (Fedora 10). Later, we'll use the built binaries from OE.

Note: loader binary dependences (in dev workstation Fedora 10)

newt-0.52.10-2.fc10.i386
slang-2.1.4-1.fc10.i386
zlib-1.2.3-18.fc9.i386
popt-1.13-4.fc10.i386
device-mapper-libs-1.02.27-6.fc10.i386 ==> LVM2 userspace device-mapper support library (libdevmapper)
libnl-1.1-5.fc10.i386 ==> library for kernel netlink sockets
dbus-libs-1.2.4-1.fc10.i386
libselinux-2.0.73-1.fc10.i386 ==> SELinux library and simple utilities
libsepol-2.0.33-1.fc10.i386 ==> SELinux binary policy manipulation library
libcap-2.10-2.fc10.i386 ==> Library for getting and setting POSIX.1e capabilities


#
# Early phase-1
#


--
  Javi Roman <javiroman@comodoo.org>



