#!/usr/bin/env bash
# vim: ts=4:sw=4:et:sts=4:ai:tw=80

# Only we can run being superuser, by now.
[ `id -u` != "0" ] && echo "You must be root!" && exit

usage () {
    echo "usage: mk-installer.sh [--clean | --cleanall]"
	exit 0
}

cleanall () {
    rm cd.iso
    rm c.img

    make -C isys clean
    make -C stage-1 clean
    make -C pyblock clean

    rm -fr CD

    rm -fr /tmp/comodoo-*
    rm -fr /tmp/dir
    rm -fr /tmp/instimage*
    rm -fr /tmp/keepfile.*

    rm yocto/bzImage-romley.bin  
    rm yocto/core-image-comodoo-romley.tar.gz  
    rm yocto/modules.tar.gz

    rm -fr logs

    exit 0
}

clean () {
    rm -fr /tmp/a-i* /tmp/instimage.*
    rm -fr CD/
    rm -f /tmp/keepfile*
    rm -fr /tmp/dir
    exit 0
}

#################
#    Main       #
#################

while [ $# -gt 0 ]; do
    case $1 in
        --debug)
            DEBUG="--debug"
            shift
            ;;
        --clean)
            clean
            ;;
        --cleanall)
            cleanall
            ;;
        --buildall)
            break
            ;;
        *)
            usage
            ;;
    esac
done

#
# Install environment requirements (tools)
#
./_mk-env.sh

#
# syslinux related stuff.
#
./_mk-bootdisk.sh
[ $? = 1 ] && \
    echo "ERROR Undefined, _mk-cdrom and _run-install skipped" && \
    exit 1

exit
#
# final ISO cd image.
#
./_mk-cdrom.sh CD

#
# run qemu installer
#
./_run-install.sh 

