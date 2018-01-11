import sys
import os
import time
import subprocess

cmd = """sfdisk -uM /dev/sda
,100,S
;
"""
pipe = subprocess.Popen(cmd, shell=True)
pipe.wait()
 
