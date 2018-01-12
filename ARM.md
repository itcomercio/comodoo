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
sudo dnf install -y libguestfs-tools virt-install

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
* Set kernel args as: console=ttyAMA0 rw root=LABEL=_/ rootwait

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

# List of ARM microarchitectures

https://en.wikipedia.org/wiki/List_of_ARM_microarchitectures

# ABI for the ARM Architecture

http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.subset.swdev.abi/index.html
