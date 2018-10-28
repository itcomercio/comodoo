#!/bin/bash
# From: https://wiki.debian.org/RepackBootableISO

usage()
{
	echo -ne "\nusage: ./makecd cd_tree_direcoty\n\n"
	exit 1
}

[ -z $1 ] && usage

xorriso -as mkisofs \
   -r -V 'Comodoo POS Installer' \
   -o cd.iso \
   -J -J -joliet-long -cache-inodes \
   -isohybrid-mbr syslinux/efi64/mbr/isohdpfx.bin \
   -b isolinux/isolinux.bin \
   -c isolinux/boot.cat \
   -boot-load-size 4 -boot-info-table -no-emul-boot \
   -eltorito-alt-boot \
   -no-emul-boot -isohybrid-gpt-basdat -isohybrid-apm-hfsplus \
   $1

