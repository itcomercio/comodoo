# ARM Emulation for POS and Devices

## References

https://fedoraproject.org/wiki/Architectures/ARM/HowToQemu

https://fedoraproject.org/wiki/Architectures/ARM/F26/Installation

https://fedoraproject.org/wiki/Architectures/ARM/Versatile_Express


# Versatile Express Boards

The Versatile Express family of development platforms provides users with a
modular board design for use testing different ARM SOC design implementations. 

This development boards by ARM can be emulated with QEMUN.


# Testing ARM Fedora Image

## Download and extraction

```
sudo dnf install -y qemu libguestfs-tools virt-install

curl -OL http://ftp.cica.es/fedora/linux/releases/26/Spins/armhfp/images/Fedora-Minimal-armhfp-26-1.5-sda.raw.xz
unxz Fedora-Minimal-armhfp-26-1.5-sda.raw.xz 

virt-builder --get-kernel Fedora-Minimal-armhfp-26-1.5-sda.raw

sudo mv Fedora-Minimal-armhfp-26-1.5-sda.raw \
    initramfs-4.11.8-300.fc26.armv7hl.img \
    vmlinuz-4.11.8-300.fc26.armv7hl \
    /var/lib/libvirt/images/
```

## Installing using Virt-Manager (Graphical )

* Start virt-manager, connect to 'QEMU', click the 'New VM' icon
* On the first page, under 'Architecture Options' select 'arm'. The 'virt'
  machine type should be selected automatically
* Select the 'Import install' option, go to the next page
* Browse to the disk image, kernel, and initrd we moved in the previous step.
* Set kernel args as: console=ttyAMA0 rw root=LABEL=________/ rootwait

## Installing using 'virt-install' (command line)

```
export LIBVIRT_PATH=/var/lib/libvirt
sudo virt-install \
        --name Fedora-Minimal-armhfp-26-1.5 \
        --ram 2048 \
        --arch armv7l \
        --import \
        --os-variant fedora26 \
        --disk $LIBVIRTPATH/images/Fedora-Minimal-armhfp-26-1.5-sda.raw \
        --boot kernel=$LIBVIRTPATH/vmlinuz-4.11.8-300.fc26.armv7hl,initrd=$LIBVIRTPATH/initramfs-4.11.8-300.fc26.armv7hl.img,kernel_args="console=ttyAMA0 rw root=LABEL=_/ rootwait" 
```

### Expanding the Disk Image

With the VM shutoff:

```
sudo qemu-img resize -f raw /var/lib/libvirt/images/Fedora-Minimal-armhfp-26-1.5-sda.raw +10G
vm # growpart /dev/sda 4
vm # resize2fs /dev/sda4
```

## Troubleshooting Note

Initial-setup

During the first boot the system will launch the 'initial-setup' utility. For
graphical images this will occur on the display, for minimal images this will
occur on the serial console. Failure to complete the initial-setup will prevent
logging into the system. To log in to the root account without completing the
initial-setup you will need to minimally edit '/etc/passwd' file and remove the
'x' from the line beginning with 'root' (this will allow you to log into the
root account without entering a password).

NOTE - currently there is a timing issue with initial-setup, even though a
display is connected, it may be run as text on the serial console. This is
often resolved by rebooting the system.

# Creating Vagrant Box from libvirt ARM image

https://github.com/vagrant-libvirt/vagrant-libvirt/issues/748

https://www.vagrantup.com/docs/boxes/base.html
https://unix.stackexchange.com/questions/222427/how-to-create-custom-vagrant-box-from-libvirt-kvm-instance

```
cp /var/lib/libvirt/images/Fedora-Minimal-armhfp-26-1.5-sda.raw workingdir/
cd workingdir
qemu-img convert -f raw -O qcow2 Fedora-Minimal-armhfp-26-1.5-sda.raw Fedora-Minimal-armhfp-26-1.5.qcow2
$HOME/.vagrant.d/gems/2.4.2/gems/vagrant-libvirt-0.0.40/tools/create_box.sh Fedora-Minimal-armhfp-26-1.5.qcow2

==> Creating box, tarring and gzipping     
./metadata.json                            
./Vagrantfile                              
./box.img
Total bytes written: 1389240320 (1.3GiB, 20MiB/s)
==> Fedora-Minimal-armhfp-26-1.5.box created
==> You can now add the box:
==>   'vagrant box add Fedora-Minimal-armhfp-26-1.5.box --name Fedora-Minimal-armhfp-26-1.5'
```

# List of ARM microarchitectures

https://en.wikipedia.org/wiki/List_of_ARM_microarchitectures
# ABI for the ARM Architecture

http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.subset.swdev.abi/index.html

# POS Prototype

The POS IZP037 is a Rockchip RK3288 board.

It is a Quad-core ARM Cortex-A17 MCore processor with separately NEON
coprocessor. A full implementation of ARM architecture v7-A instruction set,
ARM Neon Advanced SIMD (single instruction, multiple data) support.

This architecture fit in the armv7hl architectures of Linux distribution
vendors, such as Fedora ARM distributions. Armv7hl (hard floating point with 
aapcs-linux ABI, for armv7).

https://en.wikipedia.org/wiki/ARM_Cortex-A17
https://developer.arm.com/products/processors/cortex-a/cortex-a17
https://fedoraproject.org/wiki/Architectures/ARM
https://lwn.net/Articles/463506/
http://www.informit.com/articles/article.aspx?p=1620207&seqNum=4

# GCC Optimization for ARM

https://gist.github.com/fm4dd/c663217935dc17f0fc73c9c81b0aa845
https://community.arm.com/tools/b/blog/posts/arm-cortex-a-processors-and-gcc-command-lines
https://www.raspberrypi.org/forums/viewtopic.php?t=107203
https://stackoverflow.com/questions/29428726/selecting-appropriate-arm-mfpu-option-in-gcc-based-on-cpu-features
https://popolon.org/gblog3/?p=261&lang=en
https://www.cnx-software.com/2011/04/22/compile-with-arm-thumb2-reduce-memory-footprint-and-improve-performance/


