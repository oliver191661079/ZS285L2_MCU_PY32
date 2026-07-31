#!/usr/bin/env python3
#
# Build Actions NVRAM config binary file
#
# Copyright (c) 2017 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#

import os
import sys
import struct
import array
import argparse
import zlib
import argparse

def boot_calc_checksum(data):
        s = sum(array.array('H',data))
        s = s & 0xffff
        return s

def boot_add_cksum(filename):
    boot_len = os.path.getsize(filename)
    print('boot loader length %d.' %boot_len)

    with open(filename, 'rb+') as f:
        f.seek(0, 0)
        data = f.read(32)
        h_magic0,h_magic1,h_load_addr,h_name,h_version, \
        h_header_size,h_header_chksm,h_data_chksm,\
        h_body_size,h_tail_size \
        = struct.unpack('III4sHHHHII',data)

        if(h_magic0 != 0x48544341 or h_magic1 != 0x41435448):
            print('boot loader header check fail.')
            sys.exit(1)

        h_body_size = boot_len - h_header_size
        f.seek(0x18, 0)
        f.write(struct.pack('<I', h_body_size))

        f.seek(h_header_size, 0)
        data = f.read(h_body_size)
        checksum = boot_calc_checksum(data)
        checksum = 0xffff - checksum
        f.seek(0x16, 0)
        f.write(struct.pack('<H', checksum))

        f.seek(0x14, 0)
        f.write(struct.pack('<H', 0))

        f.seek(0x0, 0)
        data = f.read(h_header_size)
        checksum = boot_calc_checksum(data)
        checksum = 0xffff - checksum
        f.seek(0x14, 0)
        f.write(struct.pack('<H', checksum))
        f.close()
        print('boot loader add cksum pass.')

def image_add_secure_header(filename):
    img_len = os.path.getsize(filename)
    with open(filename, 'rb+') as f:
        f.seek(0, 0)
        data = f.read(32)
        h_magic0,h_magic1,h_load_addr,h_name,h_version, \
        h_header_size,h_header_chksm,h_data_chksm,\
        h_body_size,h_tail_size \
        = struct.unpack('III4sHHHHII',data)

        if(h_magic0 != 0x48544341 or h_magic1 != 0x41435448):
            print('boot loader header check fail.')
            sys.exit(1)

        h_body_size = img_len - h_header_size
        f.seek(0x18, 0)
        f.write(struct.pack('<I', h_body_size))
        f.seek(0x1c, 0)
        f.write(struct.pack('<I', 256))

        f.seek(h_header_size, 0)
        data = f.read(h_body_size)
        checksum = boot_calc_checksum(data)
        checksum = 0xffff - checksum
        f.seek(0x16, 0)
        f.write(struct.pack('<H', checksum))

        f.seek(0x14, 0)
        f.write(struct.pack('<H', 0))

        f.seek(0x0, 0)
        data = f.read(h_header_size)
        checksum = boot_calc_checksum(data)
        checksum = 0xffff - checksum
        f.seek(0x14, 0)
        f.write(struct.pack('<H', checksum))
        f.close()

def main(argv):
    parser = argparse.ArgumentParser(
        description='add checksum for file',
    )
    parser.add_argument('-c', dest = 'fw_file')
    parser.add_argument('-m', dest = 'mode')
    args = parser.parse_args();

    fw_file = args.fw_file
    mode = args.mode

    if mode == 'True':
        image_add_secure_header(fw_file)
    else:
        boot_add_cksum(fw_file)

if __name__ == "__main__":
    main(sys.argv)
