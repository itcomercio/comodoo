#!/bin/bash

if [ ! -a c.img ]; then
	qemu-img create -f qcow2 c.img 1000M
fi

qemu-system-x86_64 -k en-us -hda c.img -cdrom cd.iso -boot d
