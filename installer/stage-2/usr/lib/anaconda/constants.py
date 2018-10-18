#
# constants.py: anaconda constants
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

import product

MSG_ROOT = "Comodoo Point of Sale Installer"
MSG_WELCOME = "Comodoo POS installer, please press the OK button " \
              "for continuing the installation process"
MSG_DISK_SET = "Wait please The installer is preparing %s disk " \
               "for installation"
MSG_PARTITION = "Partitioning in progress ..."
MSG_PART_DONE = "The disk was sucessfully formated"
MSG_BOOTPART_READY = "Boot partition. Ready to format the Boot partition"
MSG_FORMAT_BOOT = "Making ext3 filesystem in boot partition ..."
MSG_FORMAT_OK = "The disk was sucessfully formated"
MSG_FORMAT_ROOT = "Making ext3 filesystem in root partition ..."
MSG_FORMAT_SWAP = "Making swap filesystem ..."
MSG_LOADING_BSP = "Loading Comodoo BSP, wait please ..."
MSG_BSP_OK = "BSP transfered sucessfully"
MSG_GRUB = "Wait please, Installing GRUB boot loader ..."
MSG_REBOOT = "Congratulations the installation is complete \n\n" \
             "Remove any installation media usaded during installation."


MIN_RAM = 64000

productName = product.productName
productVersion = product.productVersion
productArch = product.productArch
productPath = product.productPath
bugzillaUrl = product.bugUrl

BSPFILE = "beetlepos-image-beetlepos.bsp"

MEGABYTE = 1024 * 1024
BOOTPARTITION = 1
INSTALLDEST = "/mnt/disk"

GRUB="""
# use the serial terminal for the console.  The --unit is specifying
# that it should use /dev/ttyS0.
# serial --unit=0 --speed=38400 --word=8 --parity=no --stop=1

# it's a dumb terminal.
# terminal --dumb serial

#splashimage=(hd0,0)/grub/splash.xpm.gz

hiddenmenu
default=0
timeout=5

title Comodoo Operating System by Comodoo.org
        root (hd0,0)
        kernel /bzImage ro root=%s3 agp=off
"""

