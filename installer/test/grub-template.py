#!/bin/env python2
# vim: ts=4:sw=4:et:sts=4:ai:tw=80

from string import Template

grub_cfg_template = """
set default=0
set timeout=5

set root=(hd0,msdos1)

menuentry "$menuentry" {
        linux /bzImage root=/dev/$dev ro
}
"""

src = Template(grub_cfg_template)

menuentry = "Comodoo Point of Sale Operating System"

list = ['first', 'second', 'third']
d = {'menuentry': menuentry, 'dev':'hda3'}

result = src.substitute(d)

print result
