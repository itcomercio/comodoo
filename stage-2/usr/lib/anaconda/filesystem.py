# filesystem.py: filesystem management
#
# Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007  Red Hat, Inc.
# All rights reserved.
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
# Author(s): Matt Wilson <msw@redhat.com>
#            Jeremy Katz <katzj@redhat.com>
#
import math
import string
import isys
import os
import resource
import posix
import stat
import errno
import parted
import sys
import struct
import time
import types
from constants import *

import gettext
_ = lambda x: gettext.ldgettext("anaconda", x)

import logging
log = logging.getLogger("anaconda")

format_tool = {"ext3":"mke2fs", "swap":"mkswap"}

class FileSystem:
    def __init__(self):
        pass

    def formatDevice(self, fstype, parition):
        log.info("Format command:  %s\n" % str(args))

        rc = ext2FormatFilesystem(args, "/dev/tty5",
                                  partition)
        if rc:
            raise SystemError

def ext2FormatFilesystem(argList, messageFile, mntpoint):
    w = windowCreator(_("Formatting"),
                          _("Formatting %s file system...") % (mntpoint,), 100)

    fd = os.open(messageFile, os.O_RDWR | os.O_CREAT | os.O_APPEND)
    p = os.pipe()
    childpid = os.fork()

    if not childpid:
        os.close(p[0])
        os.dup2(p[1], 1)
        os.dup2(fd, 2)
        os.close(p[1])
        os.close(fd)

        env = os.environ
        env['MKE2FS_CONFIG'] = "/etc/mke2fs.conf"

        os.execvpe(argList[0], argList, env)
        log.critical("failed to exec %s", argList)
        os._exit(1)

    os.close(p[1])

    # ignoring SIGCHLD would be cleaner then ignoring EINTR, but
    # we can't use signal() in this thread?

    s = 'a'
    while s and s != '\b':
        try:
            s = os.read(p[0], 1)
        except OSError, args:
            (num, str) = args
            if (num != 4):
                raise IOError, args

        os.write(fd, s)

    num = ''
    while s:
        try:
            s = os.read(p[0], 1)
            os.write(fd, s)

            if s != '\b':
                try:
                    num = num + s
                except:
                    pass
            else:
                if num and len(num):
                    l = string.split(num, '/')
                    try:
                        val = (int(l[0]) * 100) / int(l[1])
                    except (IndexError, TypeError):
                        pass
                    else:
                        w and w.set(val)
                num = ''
        except OSError, args:
            (errno, str) = args
            if (errno != 4):
                raise IOError, args

    try:
        (pid, status) = os.waitpid(childpid, 0)
    except OSError, (num, msg):
        log.critical("exception from waitpid while formatting: %s %s" %(num, msg))
        status = None
    os.close(fd)

    w and w.pop()

    # *shrug*  no clue why this would happen, but hope that things are fine
    if status is None:
        return 0

    if os.WIFEXITED(status) and (os.WEXITSTATUS(status) == 0):
        return 0

    return 1




# vim: ts=4:sw=4:et:sts=4:ai:tw=80
