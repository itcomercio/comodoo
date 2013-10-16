#!/bin/bash
#
# mk-installer.sh
#
# Copyright (C) 2013 Redoop.org  All rights reserved.
# Javi Roman <javiroman@redoop.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

usage () {
    echo "usage: mk-installer.sh [<cleanall>]"
	exit 0
}

cleanall () {
    rm -f build.log

    make -C isys clean
    make -C stage-1 clean

    rm -fr CD

    rm -fr /tmp/rosi-*
    rm -fr /tmp/dir
    rm -fr /tmp/instimage*
    rm -fr /tmp/keepfile.*

    exit 0
}

clean () {
    rm -fr /tmp/a-i* /tmp/instimage.*
    rm -fr CD/
    rm -f /tmp/keepfile*
    rm -fr /tmp/dir
    exit 0
}

#
# Main
#

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
# syslinux related stuff.
#
./_mk-bootdisk.sh

#
# final ISO cd image.
#
./_mk-cdrom.sh CD

#
# run qemu installer
#
./_run-install.sh 

# vim: ts=4:sw=4:et:sts=4:ai:tw=80
