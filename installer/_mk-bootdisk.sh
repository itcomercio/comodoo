#!/usr/bin/env bash
# vim: ts=4:sw=4:et:sts=4:ai:tw=80

source include/variables.env
source include/functions.env

#############################################################
#                Functions
#############################################################
#
# set_value_distro
#
set_value_distro () {
    case $DISTRO in
    "Ubuntu") 
        value="$1"
        ;;
    "CentOS")
        value="$2"
        ;;
    "Fedora")
        value="$2"
        ;;
    *)
        echo "No distro foundª"
        exit 1
        ;;
    esac
}

#
# extract_to_keepfile
#
extract_to_keepfile() {

    set_value_distro "dpkg -L" "rpm -ql"

    $value $1 | grep -v -E "pyo|doc|man"  > tmp/in-keep.tmp
    for i in $(cat tmp/in-keep.tmp);do
        if [ -d $i ]; then
            continue
        else
            echo $i
        fi
    done
    #rm -f tmp/in-keep.tmp 2> /dev/null
}

#
# get_dso_deps
#
get_dso_deps() {
    root="$1"
    bin="$2"
    DSO_DEPS=""

    declare -a FILES
    declare -a NAMES

    # this is a hack, but the only better way requires binutils or elfutils
    # be installed.  i.e., we need readelf to find the interpretter.
    if [ -z "$LDSO" ]; then
        for ldso in ${root}/lib/ld*.so* ; do
            [ -L $ldso ] && continue
            [ -x $ldso ] || continue
            $ldso --verify $bin >/dev/null 2>&1 || continue
            LDSO=$(echo $ldso |sed -e "s,$root,,")
        done
    fi

    [[ "$bin" =~ "grub" ]] && bin="/usr/sbin/grub-probe"

    declare -i n=0
    while read NAME I0 FILE ADDR I1 ; do
        [ "$FILE" == "not" ] && FILE="$FILE $ADDR"
        NAMES[$n]="$NAME"
        FILES[$n]="$FILE"
        let n++
    done << EOF
$(env LD_TRACE_PRELINKING=1 LD_WARN= LD_TRACE_LOADED_OBJECTS=1 $LDSO $root/$bin)
EOF

    [ ${#FILES[*]} -eq 0 ] && return 1

    # we don't want the name of the binary in the list
    if [ "${FILES[0]}" == "$bin" ]; then
        FILES[0]=""
        NAMES[0]=""
        [ ${#FILES[*]} -eq 1 ] && return 1
    fi

    declare -i n=0
    while [ $n -lt ${#FILES[*]} ]; do
        FILE="${FILES[$n]}"
        if [ "$FILE" == "not found" ]; then
            cat 1>&2 <<EOF
There are missing files on your system.  The dynamic object $bin
requires ${NAMES[$n]} n order to properly function.  mkinitrd cannot continue.
EOF
            exit 1
        fi
       case "$FILE" in
         /lib*)
           TLIBDIR=`echo "$FILE" | sed 's,\(/lib[^/]*\)/.*$,\1,'`
           BASE=`basename "$FILE"`
           # Prefer nosegneg libs over direct segment accesses on i686.
           if [ -f "$TLIBDIR/i686/nosegneg/$BASE" ]; then
             FILE="$TLIBDIR/i686/nosegneg/$BASE"
           # Otherwise, prefer base libraries rather than their optimized
           # variants.
           elif [ -f "$TLIBDIR/$BASE" ]; then
             FILE="$TLIBDIR/$BASE"
           fi
           ;;
       esac
        dynamic="yes"
        let n++
    done

    DSO_DEPS="${FILES[@]}"

    for l in $(find /lib -maxdepth 1 -type l -name ld*.so*); do
       [ "$(readlink -f $l)" == "$LDSO" ] && DSO_DEPS="$DSO_DEPS $l"
    done

    [ -n "$DEBUG" ] && echo "DSO_DEPS for $bin are: $DSO_DEPS"
}

#
# instbin: Copy binary files with their dependecies
#
# instbin <root-fin-folder> <path-bin-file> <root-dest-folder> <final-dest-bin-name>
#
instbin() {
    ROOT=$1
    BIN=$2
    DIR=$3
    DEST=$4

    echo ROOT=$ROOT
    echo BIN=$BIN
    echo DIR=$DIR
    echo DEST=$DEST

    if [[ -x "$ROOT/$BIN" ]] && file "$ROOT/$BIN" | grep -q "script"
    then
        echo "install -m 755 $ROOT/$BIN $DIR/$DEST"
        install -m 755 $ROOT/$BIN $DIR/$DEST
    else
        echo "install -s -m 755 $ROOT/$BIN $DIR/$DEST"
        install -s -m 755 $ROOT/$BIN $DIR/$DEST
    fi

    # NO STRIP FOR TESTING: install -m 755 $ROOT/$BIN $DIR/$DEST

    echo get_dso_deps $ROOT $BIN
    get_dso_deps $ROOT $BIN

    local DEPS="$DSO_DEPS"
    mkdir -pv $DIR/lib
    for x in $DEPS ; do
        echo cp -Lfp $x $DIR/lib
        cp -Lfp $x $DIR/lib
    done

    pushd $DIR/lib > /dev/null
    if [ -f ld-linux.so.2 ]; then
        rm -f ld-linux.so.2
        linker="$(ls -1 ld-*.*.*.so 2> /dev/null)"
        if [ -z "$linker" ]; then
            linker="$(ls -1 ld-*.*.so 2> /dev/null)"
        fi
        found=$(echo $linker | wc -l)
        if [ $found -ne 1 ]; then
            echo "Found too many dynamic linkers:" >&2
            echo $linker >&2
            exit 1
        fi
        ln -s $linker ld-linux.so.2
    fi
    popd > /dev/null
}


#
# makeproductfile:
#
make_product_file() {
    root=$1

    echo "making stamp file in $root"
    rm -f $root/.buildstamp
    echo $IMAGEUUID > $root/.buildstamp
    echo $PRODUCT >> $root/.buildstamp
    echo $VERSION >> $root/.buildstamp
    if [ -n "$BUGURL" ]; then
        echo $BUGURL >> $root/.buildstamp
    fi
}

make_main_image () {
    imagename=$1
    type=$2
    mmi_tmpimage=$TMP_DIR/instimage.img.$$
    mmi_mntpoint=$TMP_DIR/instimage.mnt.$$
    
    rm -rf $mmi_tmpimage $mmi_mntpoint
    mkdir $mmi_mntpoint

    if [ $type = "ext2" ]; then
        SIZE=$(du -sk $IMGPATH | awk '{ print int($1 * 1.1) }')

        if [ -d $IMGPATH/usr/lib/anaconda-runtime ]; then
            ERROR=$(du -sk $IMGPATH/usr/lib/anaconda-runtime | awk '{ print $1 }')
            SIZE=$(expr $SIZE - $ERROR)
        fi

        if [ -d $IMGPATH/usr/lib/syslinux ]; then
            ERROR=$(du -sk $IMGPATH/usr/lib/syslinux | awk '{ print $1 }')
            SIZE=$(expr $SIZE - $ERROR)
        fi

        echo "create room with /dev/zero, size: $SIZE"
        dd if=/dev/zero bs=1k count=${SIZE} of=$mmi_tmpimage 2>/dev/null
        echo "create ext2 filesystem at $mmi_tmpimage"
        mke2fs -q -F $mmi_tmpimage > /dev/null
        echo "tune ext2 filesystem at $mmi_tmpimage"
        tune2fs -c0 -i0 $mmi_tmpimage >/dev/null

        echo "mount loopback $mmi_tmpimage at $mmi_mntpoint"
        #mount -o loop $mmi_tmpimage $mmi_mntpoint
        mknod -m 0660 /dev/loop2 b 7 2
        chown root:disk /dev/loop2
        losetup /dev/loop2 $mmi_tmpimage
        mount /dev/loop2 $mmi_mntpoint

        echo "cd $IMGPATH and grep files"
        cd $IMGPATH; find . | cpio -H crc -o | (cd $mmi_mntpoint; cpio -iumd)
        cd -

        make_product_file $mmi_mntpoint
        umount $mmi_mntpoint
        losetup -d /dev/loop2
        rmdir $mmi_mntpoint
    elif [ $type = "squashfs" ]; then
        make_product_file $IMGPATH
        echo "Running mksquashfs $IMGPATH $mmi_tmpimage -all-root -no-fragments -no-progress"
        mksquashfs $IMGPATH $mmi_tmpimage -all-root -no-fragments -no-progress 
        chmod 0644 $mmi_tmpimage
        SIZE=$(expr `cat $mmi_tmpimage | wc -c` / 1024)
    elif [ $type = "cramfs" ]; then
        make_product_file $IMGPATH
        echo "Running mkcramfs $CRAMBS $IMGPATH $mmi_tmpimage"
        mkfs.cramfs $CRAMBS $IMGPATH $mmi_tmpimage
        SIZE=$(expr `cat $mmi_tmpimage | wc -c` / 1024)
    fi
    
    cp $mmi_tmpimage $INSTIMGPATH/${imagename}.img
    chmod 644 $INSTIMGPATH/${imagename}.img

    echo "Wrote $INSTIMGPATH/${imagename}.img (${SIZE}k)"
    relpath=${INSTIMGPATH#$TOPDESTPATH/}
    echo "mainimage = ${relpath}/${imagename}.img" >> $TOPDESTPATH/.treeinfo
    
    rm $mmi_tmpimage
}

#
# instFile:
#
instFile() {
    FILE=$1
    DESTROOT=$2

    #[ -n "$DEBUG" ] && echo "Installing file $FILE"
    echo "Installing file $FILE"

    # if file exits or is link in destination return
    if [ -e $DESTROOT/$FILE -o -L $DESTROOT/$FILE ]; then
        return
    # create trailing folder/s
    elif [ ! -d $DESTROOT/`dirname $FILE` ]; then
        mkdir -pv $DESTROOT/`dirname $FILE`
    fi

    # if origin file is a link, copy the linked file
    if [ -L $FILE ]; then
        # Create the symlink itself
        cp -a --preserve=links $FILE $DESTROOT/`dirname $FILE`
        # copy the target of link
        instFile `dirname $FILE`/`readlink $FILE` $DESTROOT
        return
    else
        cp -aL $FILE $DESTROOT/`dirname $FILE`
    fi

    file $FILE | egrep -q ": (setuid )?ELF" &&  {
        get_dso_deps $(pwd) "$FILE"
        local DEPS="$DSO_DEPS"
        for x in $DEPS ; do
            instFile $x $DESTROOT
        done
    }
}

#
# instDir:
#
instDir() {
    DIR=$1
    DESTROOT=$2

    echo "Installing folder $DIR recursively"
    if [ -d $DESTROOT/$DIR -o -h $DESTROOT/$DIR ]; then
      return
    fi
    cp -av --parents $DIR $DESTROOT/
}

#
# make_second_stage: this is the population of install.img used in the
# installation process based on anaconda python installer.
#
make_second_stage() {
    echo "[stage2]" >> $TOPDESTPATH/.treeinfo
    echo_note "WARNING" "[016] Second stage population python based (stage-2)"
    echo_note "WARNING" "[017] Building install.img"

KEEPFILE=${TMP_DIR:-tmp}/keepfile.$$
cat > $KEEPFILE <<EOF
/usr/share/dbus-1/
/usr/lib/dbus-1.0/
/usr/lib/x86_64-linux-gnu/nss/libfreebl3.so
/usr/lib/x86_64-linux-gnu/libnss3.so
/lib/x86_64-linux-gnu/libnss_dns.so.*
/lib/x86_64-linux-gnu/libnss_files*
/usr/lib/x86_64-linux-gnu/nss/libnssckbi.so*
/usr/lib/x86_64-linux-gnu/libsmime3.so
/usr/lib/x86_64-linux-gnu/nss/libsoftokn3.so
/usr/lib/x86_64-linux-gnu/libssl3.so*
/usr/lib/x86_64-linux-gnu/libnsl.so
/usr/bin/bash
/usr/bin/cat
/usr/bin/chmod
/usr/bin/cp
/usr/bin/cpio
/usr/bin/dbus-daemon
/usr/bin/dbus-uuidgen
/usr/bin/dd
/usr/bin/df
/usr/bin/du
/usr/bin/fdisk*
/usr/bin/ln
/usr/bin/ls
/usr/bin/mkdir
/usr/bin/mount
/usr/bin/mv
/usr/bin/ps
/usr/bin/rm
/usr/bin/sed
/usr/bin/touch
/usr/bin/umount
/usr/bin/find
/usr/bin/reset
/usr/bin/gzip
/usr/sbin/arping
/sbin/*gfs*
/sbin/badblocks
/sbin/clock
/sbin/consoletype
/sbin/cryptsetup
/sbin/debugfs
/sbin/dhclient
/sbin/dhclient-script
/sbin/dosfslabel
/sbin/dumpe2fs
/sbin/e2fsadm
/sbin/e2fsck
/sbin/e2label
/sbin/fdisk
/sbin/fsck
/sbin/fsck.ext2
/sbin/fsck.ext3
/sbin/hdparm
/sbin/hwclock
/sbin/ifconfig
/sbin/ip
/sbin/iscsiadm
/sbin/iscsid
/sbin/iscsistart
/sbin/ldconfig
/sbin/lspci
/sbin/lvm*
/sbin/mdadm
/sbin/mkdosfs
/sbin/mke2fs
/sbin/mkfs.ext2
/sbin/mkfs.ext3
/sbin/mkfs.msdos
/sbin/mkfs.vfat
/sbin/mkraid
/sbin/mkswap
/sbin/mount.nfs*
/sbin/parted
/sbin/pcmcia-socket-startup
/sbin/pdisk
/sbin/probe
/sbin/setfiles
/sbin/sfdisk
/sbin/tune2fs
/sbin/udev*
/sbin/umount.nfs*
/usr/sbin/grub
/usr/bin/which
/usr/lib/NetworkManager
/usr/lib/dri
/usr/lib/gconv
/usr/lib/hal
/usr/lib/libuser/*
/usr/lib/libsqlite3.so*
/usr/bin/chattr*
/usr/bin/hattrib
/usr/bin/hcopy
/usr/bin/head
/usr/bin/hformat
/usr/bin/hmount
/usr/bin/humount
/usr/bin/chown
/usr/bin/logger
/usr/bin/lsattr*
/usr/bin/lshal
/usr/bin/vim
/usr/bin/mkzimage
/usr/bin/reduce-font
/usr/bin/tac
/usr/bin/tail
/usr/bin/coreutils
/sbin/udevadm
/usr/bin/uniq
/usr/lib/yaboot
/usr/libexec/convertdb1
/usr/libexec/hal*
/usr/libexec/nm-crash-logger
/usr/libexec/nm-dhcp-client.action
/usr/libexec/nm-dispatcher.action
/usr/sbin/NetworkManager
/usr/sbin/nm-system-settings
/usr/sbin/addRamDisk
/usr/sbin/chroot
/usr/sbin/ddcprobe
/usr/sbin/dmidecode
/usr/sbin/efibootmgr
/usr/sbin/fbset
/usr/sbin/genhomedircon
/usr/sbin/gptsync
/usr/sbin/hald
/usr/sbin/load_policy
/usr/sbin/lvm
/usr/sbin/mkofboot
/usr/sbin/ofpath
/usr/sbin/prelink
/usr/sbin/semodule
/usr/sbin/showpart
/usr/sbin/smartctl
/usr/sbin/wrapper
/usr/sbin/ybin
/usr/share/anaconda/anaconda.conf
/usr/share/cracklib
/usr/share/dbus-1
/usr/share/hal
/usr/share/hwdata/MonitorsDB
/usr/share/hwdata/pci.ids
/usr/share/hwdata/usb.ids
/usr/share/hwdata/videoaliases
/usr/share/hwdata/videodrivers
/usr/share/system-config-date
/usr/share/system-config-date/zonetab.py*
/usr/share/system-config-keyboard
/lib/terminfo/l/linux
/lib/terminfo/v/vt100
/lib/terminfo/v/vt220
/lib/libnewt.so.0.52
/usr/share/zenity
/usr/share/zoneinfo/zone.tab
/var/cache/hald
/var/lib/PolicyKit*
/var/lib/dbus
/var/lib/hal
/var/lib/misc/PolicyKit*
/var/run/PolicyKit
/var/run/dbus
/lib/terminfo
/lib/udev
/lib/x86_64-linux-gnu/libslang.so.*
/usr/lib/x86_64-linux-gnu/libnl.so.*
/lib/libdmraid.so.*
/lib/libdevmapper.so.*
/etc/NetworkManager/nm-system-settings.conf
/etc/NetworkManager/dispatcher.d
/etc/PolicyKit/*
/etc/dbus-1/*
/etc/dbus-1/system.d/*
/etc/fb.modes
/etc/fonts
/etc/group
/etc/hal
/etc/imrc
/etc/iscsid.conf
/etc/mke2fs.conf
/etc/nsswitch.conf
/etc/passwd
/etc/shadow
/etc/login.defs
/etc/prelink.conf
/etc/protocols
/etc/rpm/macros.prelink
/etc/selinux/targeted
/etc/services
/etc/shells
/etc/sysconfig/network-scripts/network-functions*
/etc/udev
/usr/share/keymaps/i386/qwerty/es.kmap.gz
/usr/share/keymaps/i386/include/*
EOF

# For further population of Python dependecies.
extract_to_keepfile python2 >> $KEEPFILE
extract_to_keepfile python2-libs >> $KEEPFILE
extract_to_keepfile python2-dialog >> $KEEPFILE
extract_to_keepfile python-parted >> $KEEPFILE
extract_to_keepfile libaudit0 >> $KEEPFILE
extract_to_keepfile vim-tiny >> $KEEPFILE
extract_to_keepfile kbd >> $KEEPFILE
extract_to_keepfile passwd >> $KEEPFILE
extract_to_keepfile glibc-minimal-langpack >> $KEEPFILE
extract_to_keepfile glibc >> $KEEPFILE
extract_to_keepfile glibc-langpack-en >> $KEEPFILE
extract_to_keepfile glibc-common >> $KEEPFILE
extract_to_keepfile glibc-headers >> $KEEPFILE
extract_to_keepfile libX11 >> $KEEPFILE
extract_to_keepfile libnl3 >> $KEEPFILE
extract_to_keepfile libxcb >> $KEEPFILE
extract_to_keepfile libXau >> $KEEPFILE
extract_to_keepfile dmraid >> $KEEPFILE
extract_to_keepfile e2fsprogs >> $KEEPFILE
extract_to_keepfile python2-dbus >> $KEEPFILE
extract_to_keepfile python2-pyparted >> $KEEPFILE
extract_to_keepfile dialog >> $KEEPFILE
extract_to_keepfile ncurses-base >> $KEEPFILE
extract_to_keepfile ncurses-libs >> $KEEPFILE
extract_to_keepfile vim-minimal >> $KEEPFILE
extract_to_keepfile udisks2 >> $KEEPFILE

rm -fr ${TOP_DIR}/tmp/dir
PKGDEST=${TOP_DIR}/tmp/dir
mkdir $PKGDEST
echo_note "WARNING" "populate $PKGDEST ..."

# TODO: nash missing
# core linux utils population
INSTALL_BIN="awk bash cat checkisomd5 chmod cp cpio cut dd \
df dhclient dmesg echo eject env grep ifconfig insmod mount.nfs \
kill less ln ls mkdir mkfs.ext3 mknod modprobe more mount mv \
rm rmmod route sed sfdisk sleep sort strace sync tree umount uniq \
ps gdb netstat test less ss coreutils"

# TODO: nash missing
# TODO: grub missing
INSTALL_SBIN=" blockdev chroot dmsetup eject fdisk \
insmod losetup lsmod mke2fs mkfs mkfs.ext2 mkfs.ext3 mkswap \
modprobe parted partprobe pidof reboot sfdisk shutdown ip \
udevadm consoletype logger"

#
# check utilities availability, abort is FAILS
#
for i in $INSTALL_BIN; do
        type $i &> /dev/null
        [ $? != 0 ] && echo "ERROR $i not in path" && exit
done
for i in $INSTALL_BIN
do
    instbin / `which $i` $PKGDEST /bin/$i &>> ${LOGS_DIR}/second-stage.log
done

#
# check utilities availability, abort is FAILS
#
for i in $INSTALL_SBIN; do
        type $i &> /dev/null
        [ $? != 0 ] && echo "ERROR $i not in path" && exit
done
for i in $INSTALL_SBIN
do
    instbin / `which $i` $PKGDEST /sbin/$i &>> ${LOGS_DIR}/second-stage.log
done

cat $KEEPFILE | while read spec ; do
    path=`echo "$spec" | sed 's,\([^[*\?]*\)/.*,\1,'`
    for filespec in `find $path -path "$spec" 2> /dev/null` ; do
        if [ ! -e $filespec ]; then
            continue
        elif [ ! -d $filespec ]; then
            instFile $filespec $PKGDEST &>> ${LOGS_DIR}/second-stage.log
        else
            for i in `find $filespec -type f 2> /dev/null` ; do 
            instFile $i $PKGDEST &>> ${LOGS_DIR}/second-stage.log ; done
            for i in `find $filespec -type l 2> /dev/null` ; do 
            instFile $i $PKGDEST &>> ${LOGS_DIR}/second-stage.log ; done
            for d in `find $filespec -type d 2> /dev/null` ; do 
            instDir $d $PKGDEST &>> ${LOGS_DIR}/second-stage.log ; done
        fi
    done
done

echo_note "OK" "[OK]"

echo_note "WARNING" "copy anaconda installer to $PKGDEST/usr/bin/"
cp -v ${TOP_DIR}/stage-2/anaconda $PKGDEST/usr/bin/
# moved usr/lib/anaconda further
cp -v ${TOP_DIR}/isys/_isys.so ${TOP_DIR}/stage-2/usr/lib/anaconda/

mv $PKGDEST/bin/* $PKGDEST/usr/bin
echo_note "OK" "[OK]"

cp -f $PKGDEST/lib64/* $PKGDEST/lib/ 2> /dev/null
rm -fr $PKGDEST/lib64

pushd $PKGDEST
ln -s lib lib64
popd

mkdir $PKGDEST/bin 
pushd $PKGDEST/bin
ln -sf ../usr/bin/bash bash
ln -sf ../usr/bin/bash sh
popd

pushd $PKGDEST/lib
ln -s libdmraid.so.1.0.0.rc16 libdmraid.so.1
popd

pushd $PKGDEST/lib
ln -s libdevmapper.so.1.02.1 libdevmapper.so.1.02
popd

# Real copy of anaconda install framework
pushd ${TOP_DIR}/stage-2
cp -a usr/ $PKGDEST
popd

# grub
echo_note "WARNING" "setting up GRUB loader staff ..."
mkdir -p $PKGDEST/usr/share/grub/

set_value_distro "/usr/lib/grub/x86_64-pc" "/usr/share/grub/i386-redhat"

cp ${value}/stage{1,2} $PKGDEST/usr/share/grub/
cat <<EOF > $PKGDEST/usr/bin/grub-install 
#!/bin/bash
/sbin/grub --batch <<! 1>/dev/null 2>/dev/null
root (hd0,0)
setup (hd0)
quit
!
EOF
chmod 755 $PKGDEST/usr/bin/grub-install
echo_note "OK" "[OK]"

rm -rf $PKGDEST/boot $PKGDEST/home $PKGDEST/root $PKGDEST/tmp

find $PKGDEST -name "*.a" | grep -v kernel-wrapper/wrapper.a | xargs rm -rf
find $PKGDEST -name "lib*.la" |grep -v "usr/$LIBDIR/gtk-2.0" | xargs rm -rf

# nuke some python stuff we don't need
for d in idle distutils bsddb lib-old hotshot doctest.py pydoc.py site-packages/japanese site-packages/japanese.pth ; do
    rm -rf $PKGDEST/$d
done

mkdir -p $PKGDEST/usr/lib/debug
mkdir -p $PKGDEST/usr/src/debug

find $PKGDEST -name "*.py" | while read fn; do
    rm -f ${fn}c
    ln -sf /dev/null ${fn}c
done

rm -fr tmp/dir/var/run/dbus/system_bus_socket

# some python stuff we don't need for install image
rm -rf $PKGDEST/usr/lib/python?.?/site-packages/distutils/
rm -rf $PKGDEST/usr/lib/python?.?/site-packages/lib-dynload/japanese
rm -rf $PKGDEST/usr/lib/python?.?/site-packages/encodings/
rm -rf $PKGDEST/usr/lib/python?.?/site-packages/compiler/
rm -rf $PKGDEST/usr/lib/python?.?/site-packages/email/test/
rm -rf $PKGDEST/usr/lib/python?.?/site-packages/curses/
rm -rf $PKGDEST/usr/lib/python?.?/site-packages/pydoc.py

# python hack
cd $PKGDEST/usr/bin
ln -sf python2.7 python
cd -

echo_note "WARNING" "Creating selected filesystem with install.img"

make_main_image "install" "ext2"
#make_main_image "install" "squashfs"

echo_note "OK" "[OK]"

[ $? = 0 ] || exit 0

}

#
# resdeps
#
resdeps () {
    items="$*"

    deplist=""
    for item in $items ; do
        deps=$(awk -F ':' "/$item.ko: / { print gensub(\".*/$item.ko: \",\"\",\"g\") }" $KERNELROOT/lib/modules/$version/modules.dep)
        for dep in $deps ; do
            depfile=${dep##*/}
            depname=${dep%%.ko}
            deplist="$deplist $depname"
        done
    done
    items=$(for n in $items $deplist; do echo $n; done | sort -u)
    echo $items
}

populate_anaconda() {
    DEST=$1

    [ ! -d $DEST ] && echo "$DEST doesn't exist! aborting." && exit 1

    cp -v stage-2/anaconda $DEST/usr/bin/
    pushd stage-2
    cp -av usr $DEST 
    popd

    echo "magic *.pyc -> /dev/null:"
    find $DEST/usr/lib/anaconda -name "*.py" | while read fn; do
        rm -f ${fn}o
        rm -f ${fn}c
        ln -sf /dev/null ${fn}c
    done

    make_main_image "install" "squashfs"

    exit 0
}

main() {
    [[ -d $TMP_DIR ]] || mkdir $TMP_DIR
    [[ -d $LOGS_DIR ]] || mkdir $LOGS_DIR
    [[ -d $STAGING_DIR ]] || mkdir $STAGING_DIR

    cat <<! > ${TMP_DIR}/setup
Comodoo installer utility. Setup:

TOP_DIR      = $TOP_DIR
TESTING      = $TESTING
DEBUG        = $DEBUG
TMP_DIR       = $TMP_DIR
MKB_DIR      = $MKB_DIR
MKB_SYSLINUX = $MKB_SYSLINUX
MKB_FSIMAGE  = $MKB_FSIMAGE
MKB_BOOTTREE = $MKB_BOOTTREE
TOPDESTPATH  = $TOPDESTPATH
INSTIMGPATH  = $INSTIMGPATH
IMGPATH      = $IMGPATH
IMAGEUUID    = $IMAGEUUID
PRODUCT      = $PRODUCT
VERSION      = $VERSION
BZIMAGE      = $BZIMAGE
ROOTFS       = $ROOTFS
MODULES      = $MODULES
DISTRO       = $DISTRO
!
}

####################################################
#                     Main 
####################################################

echo_note "WARNING" "Old temporary files housekepping ..."
rm -fr ${TMP_DIR}/* 2> /dev/null
echo_note "OK" "[OK]"

main && cat ${TMP_DIR}/setup

set -x
exec 5> ${LOGS_DIR}/${BASH_LOG_FILE}
BASH_XTRACEFD="5"
PS4='$LINENO: '

[ ! -z $1 ] && populate_anaconda $1

#
# Zero stage population (stage-0): syslinux phase
#
echo_note "WARNING" "[001] Making folder scheme ..."
mkdir -p ${INSTIMGPATH}
mkdir -p ${MKB_SYSLINUX}
mkdir -p ${MKB_DIR}/sbin
mkdir -p ${MKB_DIR}/bin
mkdir -p ${MKB_DIR}/dev
mkdir -p ${MKB_DIR}/etc
mkdir -p ${MKB_DIR}/etc/rc.d/init.d
mkdir -p ${MKB_DIR}/etc/udev/rules.d
mkdir -p ${MKB_DIR}/lib/udev/rules.d
mkdir -p ${MKB_DIR}/etc/modprobe.d
mkdir -p ${MKB_DIR}/etc/terminfo/{a,b,d,l,s,v,x}
mkdir -p ${MKB_DIR}/etc/sysconfig/network-scripts
mkdir -p ${MKB_DIR}/usr/libexec/dbus-1
mkdir -p ${MKB_DIR}/usr/sbin
mkdir -p ${MKB_DIR}/usr/bin
mkdir -p ${MKB_DIR}/usr/lib/systemd
mkdir -p ${MKB_DIR}/proc
mkdir -p ${MKB_DIR}/selinux
mkdir -p ${MKB_DIR}/sys
mkdir -p ${MKB_DIR}/tmp
mkdir -p ${MKB_DIR}/var/lib/dhclient
mkdir -p ${MKB_DIR}/var/run
mkdir -p ${MKB_DIR}/var/lib/misc
mkdir -p ${MKB_DIR}/usr/share/grub/
mkdir -p ${MKB_DIR}/etc/dbus-1/system.d
mkdir -p ${MKB_DIR}/usr/share/dbus-1/system-services
mkdir -p ${MKB_DIR}/lib/dbus-1
mkdir -p ${MKB_DIR}/var/run/dbus
mkdir -p ${MKB_DIR}/var/lib/dbus
mkdir -p ${MKB_DIR}/etc/NetworkManager/dispatcher.d
mkdir -p ${MKB_DIR}/usr/lib/NetworkManager
mkdir -p ${MKB_DIR}/var/run/NetworkManager
mkdir -p ${MKB_DIR}/etc/PolicyKit
mkdir -p ${MKB_DIR}/usr/share/PolicyKit/policy
mkdir -p ${MKB_DIR}/etc/hal/fdi
mkdir -p ${MKB_DIR}/usr/share/hal/fdi
mkdir -p ${MKB_DIR}/usr/share/hwdata
mkdir -p ${MKB_DIR}/var/cache/hald
mkdir -p ${MKB_DIR}/run/dbus
mkdir -p ${MKB_DIR}/run/udev
echo_note "OK" "[OK]"

#
# Builds "syslinux" bootloader utilities
#
echo_note "WARNING" "[003] syslinux building ...."
echo "############### syslinux #########" >> ${LOGS_DIR}/build.log
make -C syslinux &>> ${LOGS_DIR}/build.log
if [ $? = 0 ];then
    echo_note "OK" "[OK]"
else
    echo_note "ERROR" "[ERROR]"
fi

#
# syslinux building phase
#
echo_note "WARNING" "[002] syslinux installation ...."
cd ${CUR_PWD}
cp -a addons/isolinux ${MKB_SYSLINUX}
cp syslinux/bios/core/isolinux.bin ${MKB_SYSLINUX}/isolinux
cp syslinux/bios/com32/menu/vesamenu.c32 ${MKB_SYSLINUX}/isolinux
cp syslinux/bios/com32/elflink/ldlinux/ldlinux.c32 ${MKB_SYSLINUX}/isolinux
cp syslinux/bios/com32/lib/libcom32.c32 ${MKB_SYSLINUX}/isolinux
cp syslinux/bios/com32/libutil/libutil.c32 ${MKB_SYSLINUX}/isolinux
echo_note "OK" "[OK]"


#
# Builds "isys" installer utility functions
#
echo_note "WARNING" "[003] isys building ...."
echo "############### isys #########" >> ${LOGS_DIR}/build.log
make -C isys &>> ${LOGS_DIR}/build.log
if [ $? = 0 ];then
    echo_note "OK" "[OK]"
else
    echo_note "ERROR" "[ERROR]"
fi

#
# Builds "isomd5sum" Utilities for working with md5sum implanted in ISO images
#
echo_note "WARNING" "[004] isomd5sum building ...."
echo "############### isomd5sum #########"  >> ${LOGS_DIR}/build.log
make -C isomd5sum &>> ${LOGS_DIR}/build.log
if [ $? = 0 ];then
    echo_note "OK" "[OK]"
else
    echo_note "ERROR" "[ERROR]"
fi

#
# Build "init" and "loader" stage-1 installer
#
echo_note "WARNING" "[005] stage-1 building...."
echo "############### stage-1 #########" >> ${LOGS_DIR}/build.log
make -C stage-1 &>> ${LOGS_DIR}/build.log
if [ $? = 0 ];then
    echo_note "OK" "[OK]"
else
    echo_note "ERROR" "[ERROR]"
fi

#
# Build "pyblock" python bindings for device-mapper functionalities
#
echo_note "WARNING" "[006] pyblock building...."
echo "############### stage-1 #########" >> ${LOGS_DIR}/build.log
make -C pyblock &>> ${LOGS_DIR}/build.log
if [ $? = 0 ];then
    echo_note "OK" "[OK]"
else
    echo_note "ERROR" "[ERROR]"
fi

#
# Populate custom "init" process
#
echo_note "WARNING" "[007] Install init in ${MKB_DIR}/sbin/init"
instbin ${PWD}/stage-1 init \
    ${MKB_DIR} /sbin/init &>> ${LOGS_DIR}/init-loader.log
echo_note "OK" "[OK]"

#
# Populate "loader" custom installer process
#
echo_note "WARNING" "[008] Install loader in ${MKB_DIR}/sbin/loader"
instbin ${PWD}/stage-1 loader \
    ${MKB_DIR} /sbin/loader &>> ${LOGS_DIR}/init-loader.log
echo_note "OK" "[OK]"

#
# hatl, reboot and poweroff set to custom "init"
#
cd ${MKB_DIR}/sbin
ln -s ./init  ${MKB_DIR}/sbin/reboot
ln -s ./init  ${MKB_DIR}/sbin/halt
ln -s ./init  ${MKB_DIR}/sbin/poweroff
cd ${TOP_DIR}

#
# Populate "pyblock" to second stage folders (used by anaconda)
#
cp ${TOP_DIR}/pyblock/dmmodule.so.0.48 \
    ${TOP_DIR}/stage-2/usr/lib/anaconda/block/dmmodule.so
cp ${TOP_DIR}/pyblock/dmraidmodule.so.0.48 \
    ${TOP_DIR}/stage-2/usr/lib/anaconda/block/dmraidmodule.so

# core linux utils population
# TODO: nash missing
INSTALL_BIN="awk bash cat checkisomd5 chmod cp cpio cut dd \
df dhclient dmesg echo eject env grep ifconfig insmod mount.nfs \
kill less ln ls mkdir mkfs.ext3 mknod modprobe more mount mv \
rm rmmod route sed sfdisk sleep sort strace sync tree umount uniq \
ps gdb netstat test less id systemctl chvt tty"

# TODO: nash missing
# TODO: grub missing
INSTALL_SBIN="blockdev chroot dmsetup eject fdisk \
insmod losetup lsmod mke2fs mkfs mkfs.ext2 mkfs.ext3 mkswap \
modprobe parted partprobe pidof reboot sfdisk shutdown ip \
udevadm consoletype logger"

echo_note "WARNING" "[009] $MKB_DIR population ..."
#
# check utilities availability, abort is FAILS
#
for i in $INSTALL_BIN; do
        type $i &> /dev/null
        [ $? != 0 ] && echo "ERROR $i not in path" && exit
done
for i in $INSTALL_BIN; do
    instbin / `which $i` $MKB_DIR /bin/$i &>> ${LOGS_DIR}/init-loader.log
done

#
# check utilities availability, abort is FAILS
#
for i in $INSTALL_SBIN; do
        type $i &> /dev/null
        [ $? != 0 ] && echo "ERROR $i not in path" && exit
done
for i in $INSTALL_SBIN; do
    instbin / `which $i` $MKB_DIR /sbin/$i &>> ${LOGS_DIR}/initrd-population.log
done

instbin / `which python` $MKB_DIR /usr/bin/python \
    &>> ${LOGS_DIR}/initrd-population.log
instbin / `which load_policy` $MKB_DIR /usr/sbin/load_policy \
    &>> ${LOGS_DIR}/initrd-population.log
instbin /usr/lib/systemd systemd-udevd $MKB_DIR /usr/lib/systemd/systemd-udev \
    &>> ${LOGS_DIR}/initrd-population.log
instbin / /usr/bin/coreutils $MKB_DIR /usr/bin/coreutils \
    &>> ${LOGS_DIR}/initrd-population.log
instbin / /usr/libexec/udisks2/udisksd $MKB_DIR /usr/libexec/udisks2/udisksd \
    &>> ${LOGS_DIR}/initrd-population.log

# NSS files for dbus-daemon
cp /lib64/libnss* ${MKB_DIR}/lib

KEEPFILERD=${TMP_DIR:-tmp}/keepfilerd.$$

extract_to_keepfile passwd >> $KEEPFILERD
extract_to_keepfile libc >> $KEEPFILERD
extract_to_keepfile libc-bin >> $KEEPFILERD
extract_to_keepfile libc6 >> $KEEPFILERD

cat $KEEPFILERD | while read spec ; do
    path=`echo "$spec" | sed 's,\([^[*\?]*\)/.*,\1,'`
    for filespec in `find $path -path "$spec" 2> /dev/null` ; do
        if [ ! -e $filespec ]; then
            continue
        elif [ ! -d $filespec ]; then
            instFile $filespec $MKB_DIR &>> ${LOGS_DIR}/initrd-population.log
        else
            for i in `find $filespec -type f 2> /dev/null`; do 
            instFile $i $MKB_DIR &>> ${LOGS_DIR}/initrd-population.log ; done
            for i in `find $filespec -type l 2> /dev/null`; do 
            instFile $i $MKB_DIR &>> ${LOGS_DIR}/initrd-population.log ; done
            for d in `find $filespec -type d 2> /dev/null`; do 
            instDir $d $MKB_DIR &>> ${LOGS_DIR}/initrd-population.log ; done
        fi
    done
done

# udev
install -m 644 /etc/udev/udev.conf $MKB_DIR/etc/udev/udev.conf
install -m 644 /etc/udev/udev.conf $MKB_DIR/etc/udev/udev.conf

for i in /lib/udev/rules.d/*.rules ; do
    install -m 644 $i $MKB_DIR/lib/udev/rules.d/${i##*/}
done
#for i in /etc/udev/rules.d/*.rules ; do
    #install -m 644 $i $MKB_DIR/etc/udev/rules.d/${i##*/}
#done
for i in /lib/udev/*; do
    if [ -f $i ]; then install -m 755 $i $MKB_DIR/lib/udev/${i##*/}; fi
done

# dbus
instbin / `which dbus-uuidgen` $MKB_DIR /sbin/dbus-uuidgen &>> ${LOGS_DIR}/initrd-population.log
instbin / `which dbus-daemon` $MKB_DIR /sbin/dbus-daemon &>> ${LOGS_DIR}/initrd-population.log

cp -a /etc/dbus-1/system.conf $MKB_DIR/etc/dbus-1/system.conf
cp -a /usr/share/dbus-1/system.conf $MKB_DIR/usr/share/dbus-1/system.conf
cp -a /etc/dbus-1/session.conf $MKB_DIR/etc/dbus-1/session.conf
cp -a -f /etc/dbus-1 $MKB_DIR/etc/

cp -v -a /usr/libexec/dbus-1/dbus-daemon-launch-helper \
    $MKB_DIR/usr/libexec/dbus-1/dbus-daemon-launch-helper

chown root:dbus $MKB_DIR/usr/libexec/dbus-1/dbus-daemon-launch-helper
chmod 04750 $MKB_DIR/usr/libexec/dbus-1/dbus-daemon-launch-helper

# i18n from anaconda
KEYMAPS=addons/keymaps-override-x86_64
SCREENFONT=addons/screenfont-x86_64.gz
MYLANGTABLE=addons/lang-table
install -m 644 $KEYMAPS $MKB_DIR/etc/keymaps.gz
install -m 644 $SCREENFONT $MKB_DIR/etc/screenfont.gz
install -m 644 $MYLANGTABLE $MKB_DIR/etc/lang-table

# terminal charset
for i in a/ansi d/dumb l/linux s/screen v/vt100 v/vt100-nav v/vt102 ; do
    [ -f /lib/terminfo/$i ] && \
        install -m 644 /usr/share/terminfo/$i $MKB_DIR/etc/terminfo/$i
done

echo_note "OK" "[OK]"

echo_note "WARNING" "[010] kernel modules population for initrd image ..."

if [ -z ${ROOTFS} ]; then
    echo_note "ERROR" "ERROR: BSP image is no available. Exiting!"
    exit 1
fi

mkdir ${TOP_DIR}/yoctotmp 2> /dev/null
mkdir -p ${TOP_DIR}/yoctotmp2/lib 2> /dev/null

echo "uncompressing ${TOP_DIR}/yocto/${ROOTFS}"
tar xvzf ${TOP_DIR}/yocto/${ROOTFS} -C yoctotmp &>> ${LOGS_DIR}/modules.log
mv ${TOP_DIR}/yoctotmp/lib/modules yoctotmp2/lib &>> ${LOGS_DIR}/modules.log
cd ${TOP_DIR}/yoctotmp2

tar cvzf ../${MODULES} lib &>> ${LOGS_DIR}/modules.log
cd ../..
rm -fr yoctotmp
rm -fr yoctotmp2
tar xzf ${TOP_DIR}/yocto/${MODULES} -C $MKB_DIR &>> ${LOGS_DIR}/modules.log
echo_note "OK" "[OK]"

echo_note "WARNING" "[011] tricky stuff population for initrd image ..."
install -m 644 /etc/passwd $MKB_DIR/etc/passwd
install -m 644 /etc/group $MKB_DIR/etc/group
install -m 644 /etc/nsswitch.conf $MKB_DIR/etc/nsswitch.conf

cp /etc/rc.d/init.d/functions $MKB_DIR/etc/rc.d/init.d

# DHCP and DHCPv6 client daemons and support programs
cp -a /sbin/dhclient-script $MKB_DIR/sbin/dhclient-script
chmod 0755 $MKB_DIR/sbin/dhclient-script
touch $MKB_DIR/etc/resolv.conf

# hwdata
#cp -a /usr/share/hwdata/pci.ids $MKB_DIR/usr/share/hwdata/pci.ids
#cp -a /usr/share/hwdata/usb.ids $MKB_DIR/usr/share/hwdata/usb.ids

# hal
#instbin / /usr/sbin/hald $MKB_DIR /sbin/hald &>>  ${TOP_DIR}/logs/initrd-population

#set_value_distro "/usr/lib/hal" "/usr/libexec"

#( cd $value
#      for f in hald-runner hald-generate-fdi-cache hal*storage* ; do
#          instbin / $value/$f $MKB_DIR /usr/libexec/$f &>> ${TOP_DIR}/logs/initrd-population
#      done
#)
#touch $MKB_DIR/var/run/hald.acl-list
#cp -a /usr/share/hal/fdi/* $MKB_DIR/usr/share/hal/fdi
#cp -a /etc/hal/fdi/* $MKB_DIR/etc/hal/fdi
#cp -a /etc/dbus-1/system.d/hal.conf $MKB_DIR/etc/dbus-1/system.d

# PolicyKit
# cp -a /etc/polkit-1 $MKB_DIR/etc/

#( cd /usr/share/dbus-1/system-services
#      cp -a org.freedesktop.PolicyKit1.service $MKB_DIR/usr/share/dbus-1/system-services
#)
#( cd /usr/share/polkit-1/actions/
#      cp -a org.freedesktop.policykit.policy $MKB_DIR/usr/share/PolicyKit/policy
#)

# Missing in polkit-1
#
#( cd /var/lib/misc
#      cp -av PolicyKit.reload $MKB_DIR/var/lib/misc
#)

# FIXME: http://projects.gnome.org/NetworkManager/developers/api/09/ref-migrating.html
# NetworkManager
instbin / /usr/sbin/NetworkManager $MKB_DIR /usr/sbin/NetworkManager &>> ${LOGS_DIR}/initrd-population.log
instbin / /usr/sbin/nm-system-settings $MKB_DIR /usr/sbin/nm-system-settings &>> ${LOGS_DIR}/initrd-population.log
cp -a /etc/dbus-1/system.d/nm-*.conf $MKB_DIR/etc/dbus-1/system.d
cp -a /etc/dbus-1/system.d/NetworkManager.conf $MKB_DIR/etc/dbus-1/system.d
cp -a /etc/NetworkManager/nm-system-settings.conf $MKB_DIR/etc/NetworkManager
( cd /usr/lib/NetworkManager
      for f in *.so ; do
          instbin / /usr/lib/NetworkManager/$f $MKB_DIR /usr/lib/NetworkManager/$f &>> ${LOGS_DIR}/initrd-population.log
      done
)
( cd /usr/libexec
      for f in nm-* ; do
          instbin / /usr/libexec/$f $MKB_DIR /usr/libexec/$f &>> ${LOGS_DIR}/initrd-population.log
      done
)
( cd /usr/share/dbus-1/system-services
      cp -a org.freedesktop.NetworkManagerSystemSettings.service $MKB_DIR/usr/share/dbus-1/system-services
      cp -a org.freedesktop.nm_dispatcher.service $MKB_DIR/usr/share/dbus-1/system-services
)

echo_note "OK" "[OK]"

echo_note "WARNING" "[012] sfdisk.in file configuration"
# Partitioning
cat > $MKB_DIR/etc/sfdisk.in <<!
,100,S
;
!
echo_note "OK" "[OK]"

# Indirect dependencies: FIXME
#install -m 755 /lib/libfreebl3.so $MKB_DIR/lib/
#install -m 755 /lib/libsoftokn3.so $MKB_DIR/lib/

#install -m 755 /usr/lib/libsqlite3.so.0 $MKB_DIR/usr/lib/
install -m 755 /usr/lib/x86_64-linux-gnu/libsqlite3.so.0.8.6 $MKB_DIR/usr/lib/

#install -m 755 /lib/libnss_dns.so.2 $MKB_DIR/lib/
install -m 755 /lib/x86_64-linux-gnu/libnss_dns.so.2 $MKB_DIR/lib/

#install -m 755 /lib/libnss_files.so.2 $MKB_DIR/lib/
install -m 755 /lib/x86_64-linux-gnu/libnss_files.so.2 $MKB_DIR/lib/

#install -m 755 /lib/libgcc_s.so.1 $MKB_DIR/lib/
install -m 755 /lib/x86_64-linux-gnu/libgcc_s.so.1 $MKB_DIR/lib/

make_product_file $MKB_DIR

echo_note "WARNING" "Creating important symlinks"

if [ ! $TESTING ]; then
    ln -s /sbin/init $MKB_DIR/init
    # workaround for x86_64 architectures
    ln -s /lib $MKB_DIR/lib64
elif [ $TESTING -eq 1 ]; then
    ln -s /sbin/init $MKB_DIR/inittest
    ln -s /lib $MKB_DIR/lib64
    cat > $MKB_DIR/init <<EOF
#!/bin/bash
emergency_shell()
{
    echo "Bug in /sbi/init detected. Dropping to a shell. Good luck!"
    echo
    bash
}

trap "emergency_shell" 0 2
# exit immediately if a command fails
set -e

export PATH=/sbin:/bin

exec < /dev/console > /dev/console 2>&1

mount -n -t proc proc /proc
mount -n -t sysfs sysfs /sys

echo "ready to run /sbin/init"
/bin/sleep 2
/sbin/init
EOF
    chmod 0755 $MKB_DIR/init
else 
    ln -s /sbin/init $MKB_DIR/init
    cp stage-1/*.c $MKB_DIR/sbin/
fi

ln -s /bin/bash $MKB_DIR/bin/sh
ln -s /proc/mounts $MKB_DIR/etc/mtab
ln -s sbin $MKB_DIR/bin
mkdir -p $MKB_DIR/var/lib
ln -s ../..tmp $MKB_DIR/var/lib/xkb

cat > $MKB_DIR/.profile <<EOF
PATH=/sbin:/bin:/usr/bin:/usr/sbin:/mnt/sysimage/sbin:/mnt/sysimage/usr/sbin:/mnt/sysimage/bin:/mnt/sysimage/usr/bin
export PATH
EOF

# initrd ready
echo_note "WARNING" "[013] Building initrd.img image ...."

#cd $MKB_DIR; find . | cpio --quiet -H newc -o | gzip -9 -n > $MKB_FSIMAGE
cd $MKB_DIR; find . | cpio --quiet -H newc -o > $MKB_FSIMAGE
cd -

size=$(du $MKB_FSIMAGE | awk '{ print $1 }')
echo_note "WARNING" "[014] Wrote $MKB_FSIMAGE (${size}k compressed)"

# checking gzip file integrity
#gzip -t $MKB_FSIMAGE
#if [ $? = 1 ]; then
    #echo_note "ERROR" "ERROR CRC initrd"
    #exit 1
#fi
echo_note "OK" "[OK]"

echo_note "WARNING" "[015] populate final boot tree ..."
[ ! -d $MKB_BOOTTREE ] && mkdir $MKB_BOOTTREE
cp -a $MKB_SYSLINUX/isolinux  $MKB_BOOTTREE/
cp -vf ${TOP_DIR}/yocto/${BZIMAGE} ${MKB_BOOTTREE}/isolinux/vmlinuz
[ ! -d $MKB_BOOTTREE/bsp ] && mkdir $MKB_BOOTTREE/bsp
cp -vf ${TOP_DIR}/yocto/${ROOTFS} ${MKB_BOOTTREE}/bsp/beetlepos-image-beetlepos.bsp
cp -vf $MKB_FSIMAGE $MKB_BOOTTREE/isolinux/initrd.img

tree ${TOP_DIR}/CD
echo_note "OK" "[OK]"

#
# Second stage population (stage-2):
#   the real installer, python based (inspired in anaconda installer)
#
make_second_stage


