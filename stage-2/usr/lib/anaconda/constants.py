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

title Redoop Operating System by Redoop.org
        root (hd0,0)
        kernel /bzImage ro root=%s3 agp=off
"""

