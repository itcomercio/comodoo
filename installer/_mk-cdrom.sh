#!/bin/bash

usage()
{
	echo -ne "\nusage: ./makecd cd_tree_direcoty\n\n"
	exit 1
}

[ -z $1 ] && usage

mkisofs -T -r -b isolinux/isolinux.bin \
        -c isolinux/boot.catalog \
        -no-emul-boot \
        -boot-load-size 4 \
        -boot-info-table \
	    -V Comodoo-POS-Installer \
	    -o cd.iso \
	    $1



