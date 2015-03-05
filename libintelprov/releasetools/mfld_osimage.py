#!/usr/bin/env python

# Copyright (C) 2011 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import struct
import getopt
import sys

# Bootstub only reserves this much space for the cmdline
max_cmdline_size = 511

def write_padded(outfile, data, padding):
    padding = padding - len(data)
    assert padding >= 0
    outfile.write(data)
    outfile.write('\0' * padding)

def make_osimage(bootstub, kernel, cmdline, ramdisk, filename):
    """Create a medfield-compatible OS image from component parts. This image
    will need to be stitched by FSTK before a medfield device will boot it"""
    kernel_sz = os.stat(kernel).st_size
    cmdline_sz = os.stat(cmdline).st_size
    ramdisk_sz = os.stat(ramdisk).st_size
    console_suppression = 0
    console_dev_type = 0xff

    kernel_f = open(kernel, "rb")
    cmdline_f = open(cmdline, "rb")
    ramdisk_f = open(ramdisk, "rb")
    bootstub_f = open(bootstub, "rb")
    outfile_f = open(filename, "wb")

    assert cmdline_sz <= max_cmdline_size, "Command line too long, max %d bytes" % (max_cmdline_size,)

    write_padded(outfile_f, cmdline_f.read(), 512)
    write_padded(outfile_f, struct.pack("<IIII", kernel_sz, ramdisk_sz,
                                    console_suppression, console_dev_type), 3584)
    write_padded(outfile_f, bootstub_f.read(), 4096)
    outfile_f.write(kernel_f.read())
    outfile_f.write(ramdisk_f.read())

    kernel_f.close()
    cmdline_f.close()
    ramdisk_f.close()
    bootstub_f.close()
    outfile_f.close()

def usage():
    print "-b | --bootstub    Bootstub binary"
    print "-k | --kernel      kernel bzImage binary"
    print "-c | --cmdline     Kernel command line file (max " + max_cmdline_size + " bytes)"
    print "-r | --ramdisk     Ramdisk"
    print "-o | --output      Output OS image"
    print "-h | --help        Show this message"
    print
    print "-b, -k, -c, -r, and -o are required"

def main():
    bootstub = None
    kernel = None
    cmdline = None
    ramdisk = None
    outfile = None
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hb:k:c:r:o:", ["help",
            "bootstub=", "kernel=", "cmdline=", "ramdisk=", "output="])
    except getopt.GetoptError, err:
        usage()
        print err
        sys.exit(2)
    for o, a in opts:
        if o in ("-h", "--help"):
            usage()
            sys.exit()
        elif o in ("-b", "--bootstub"):
            bootstub = a
        elif o in ("-k", "--kernel"):
            kernel = a
        elif o in ("-c", "--cmdline"):
            cmdline = a
        elif o in ("-r", "--ramdisk"):
            ramdisk = a
        elif o in ("-o", "--output"):
            outfile = a
        else:
            usage()
            sys.exit(2)
    if (not bootstub or not kernel or not cmdline or not ramdisk or not outfile):
        usage()
        print "Missing required option!"
        sys.exit(2)
    make_osimage(bootstub, kernel, cmdline, ramdisk, outfile)

if __name__ == "__main__":
    main()

