#!/bin/bash

if [ ! -a c.img ]; then
	qemu-img create -f qcow2 c.img 1000M
fi

qemu-system-x86_64 -k en-us -hda c.img -cdrom cd.iso -boot d -m size=2048

# debugging early crash.
qemu-system-x86_64 -k en-us -hda c.img -cdrom cd.iso -boot d -m size=2048 -kernel CD/isolinux/vmlinuz -initrd CD/isolinux/initrd.img  -serial stdio -append "root=/dev/ram0 console=ttyAMA0  console=ttyS0"
