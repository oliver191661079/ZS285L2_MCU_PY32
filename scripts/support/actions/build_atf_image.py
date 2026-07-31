#!/usr/bin/env python3
#
# Build ATF image file
#
# Copyright (c) 2019 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#

import os
import sys
import struct
import argparse
import zlib
import re

ATF_DATA_ALIGN_SIZE = 512
ATF_MAX_FILE_CNT = 15
ATF_VERSION = 0x01000000
ATF_HEAD_SIZE = 32
ATF_DIR_SIZE = 32

'''
/* ATF image */
typedef struct
{
    uint8 magic[8];
    uint8 sdk_version[4];
    uint8 file_total;
    uint8 reserved[19];
    atf_dir_t atf_dir[15];
}atf_head_t;

typedef struct
{
    uint8 filename[12];
    uint8 reserved1[4];//load_addr
    uint32 offset;
    uint32 length;
    uint8 reserved2[4];//run_addr
    uint32 checksum;
}atf_dir_t;
'''

class atf_dir:
    def __init__(self):
        self.name = ''
        self.offset = 0
        self.length = 0
        self.aligned_length = 0
        self.checksum = 0
        self.load_addr = 0
        self.run_addr = 0
        self.data = None

atf_file_list = []
config_str_list = []
atf_header = None
cur_data_offset = ATF_DATA_ALIGN_SIZE

def atf_add_crc(filename):
    if os.path.isfile(filename):
        # update data crc
        with open(filename, 'rb+') as f:
            #skip magic header
            f.read(512)
            crc = zlib.crc32(f.read(), 0) & 0xffffffff
            #update crc field in header
            f.seek(24)
            f.write(struct.pack('<I',crc))

        # update head crc
        with open(filename, 'rb+') as f:
            #skip magic header
            f.read(12)
            crc = zlib.crc32(f.read(512 - 12), 0) & 0xffffffff
            #update crc field in header
            f.seek(8)
            f.write(struct.pack('<I',crc))


def atf_add_file(fpath):
    global atf_file_list, cur_data_offset
    global config_str_list

    ConfigPattern = re.compile(r'''
            ^                       # beginning of string
            \s*                     # is optional and can be any number of whitespace character
            (ATT_PATTERN_FILE|ATT_BOOT_FILE|ATT_CONFIG_XML|ATT_CONFIG_TXT|ATT_DFU_FW)
            \s*                     # is optional and can be any number of whitespace character
            =                       # key word '='
            \s*                     # is optional and can be any number of whitespace character
            (.+?)                   # config string
            $                       # end of string
            ''', re.VERBOSE + re.IGNORECASE)

    if not os.path.isfile(fpath):
        print('ATF: file %s is not exist' %(fpath));
        return False

    with open(fpath, 'rb') as f:
        fdata = f.read()

    af = atf_dir()

    af.name = os.path.basename(fpath)
    # check filename: 8.3
    if len(af.name) > 12:
        print('ATF: file %s name is too long' %(fpath))
        return False

    af.length = len(fdata)
    if af.length == 0:
        print('ATF: file %s length is zero' %(fpath))
        return False

    for i, config_string in enumerate(config_str_list):
        ConfigObject = ConfigPattern.search(config_string)
        if ConfigObject:
            config_tupe = ConfigObject.groups()
            if config_tupe[0] == 'ATT_PATTERN_FILE':
                str_list = config_tupe[1].strip().split(',')
                if os.path.basename(str_list[0].strip()) == af.name :
                    # print("*****************")
                    # print(af.name)
                    load_addr = str_list[1].strip()
                    run_addr = str_list[2].strip()
                    if load_addr.startswith('0x'):
                        af.load_addr = int(load_addr[2:], 16)

                    if run_addr.startswith('0x'):
                        af.run_addr = int(run_addr[2:], 16)

                    # print(type(af.load_addr),af.load_addr)
                    # print( type(af.run_addr),af.run_addr)
                    # print("")
        else:
            print("NOT support key word: %s in line %d of file %s." %
                (config_string, i, self.config_file))

    padsize = ATF_DATA_ALIGN_SIZE - af.length % ATF_DATA_ALIGN_SIZE
    af.aligned_length = af.length + padsize

    paddata = bytearray(padsize)
    for i in range(0,padsize):
        paddata[i] = 0xff
    af.data = fdata + paddata
    af.offset = cur_data_offset

    # caculate file crc
    af.checksum = zlib.crc32(fdata, 0) & 0xffffffff

    cur_data_offset = cur_data_offset + af.aligned_length

    atf_file_list.append(af)

    return True

def atf_gen_header():
    global atf_header
    atf_dir = bytearray(0)

    if len(atf_file_list) == 0:
        return;
    for af in atf_file_list:
        atf_dir = atf_dir + struct.pack('<12sIIIII', bytearray(af.name, 'utf8'), \
                    af.load_addr,af.offset, af.length, af.run_addr,af.checksum)

    total_len = cur_data_offset

    atf_header = bytearray("ACTTEST0", 'utf8') + \
                            struct.pack('<IB3xIII4x', 0, len(atf_file_list), ATF_VERSION, total_len, 0)

    atf_header = atf_header + atf_dir
    padsize = ATF_DATA_ALIGN_SIZE - len(atf_header) % ATF_DATA_ALIGN_SIZE
    atf_header = atf_header + bytearray(padsize)

def extract_image(image_file, out_dir):
    if not os.path.isfile(image_file):
        print('ATF: file %s is not exist' %(image_file));
        return False

    print('ATF: extract %s to %s' %(image_file, out_dir))

    if not os.path.exists(out_dir):
        os.makedirs(out_dir)

    f = open(image_file, 'rb')
    if f == None:
        return False

    f.seek(0, 0)
    magic = f.read(8)

    if magic != b'ACTTEST0':
        print('invalid magic')
        return False;

    f.seek(0xc, 0)
    file_cnt, = struct.unpack('B', f.read(1))
    if file_cnt > ATF_MAX_FILE_CNT:
        print(ATF_MAX_FILE_CNT)
        print(file_cnt)
        print('ATF: invalid file count %d' %(file_cnt))
        return False

    print('ATF: file count %d' %(file_cnt))

    i = 0
    while i < file_cnt:
        f.seek(ATF_HEAD_SIZE + i * ATF_DIR_SIZE, 0)
        i = i + 1

        name, offset, length, checksum = \
            struct.unpack('<12s4xII4xI', f.read(0x20))

        if offset != 0 and length != 0:
            file_name = os.path.join(out_dir, name.decode('utf8').rstrip('\0'))
            print('ATF: extract file %s, length 0x%x' %(file_name, length))

            with open(file_name, 'wb') as wf:
                f.seek(offset, 0);
                wf.write(f.read(length))

    return True

def PreparseConfig(input_file):
    """Preprocess input file, filter out all comment line string.
    """
    config_list = []

    if os.path.isfile(input_file):
        with open(input_file, 'r') as p_file:
            str             = p_file.read()
            line_str_list   = str.split('\n')

            for line_str in line_str_list:
                line_str = line_str.strip()
                # filter out comment string and empty line
                if line_str and not re.search(r'^\s*($|//|#)', line_str):
                    config_list.append(line_str)

    return config_list

def main(argv):
    global atf_header, cur_data_offset
    global config_str_list

    parser = argparse.ArgumentParser(
        description='Build ATT firmware image',
    )
    parser.add_argument('-x', dest = 'extract_out_dir')
    parser.add_argument('-i', dest = 'image_file')
    parser.add_argument('-o', dest = 'output_file')
    parser.add_argument('input_files', nargs = '*')
    parser.add_argument('-cf', dest='config_file')
    args = parser.parse_args();

    print('ATF: Build ATT firmware image: %s' %args.output_file)
    print(args.config_file)
    if (args.output_file) :

        #count header len
        cur_data_offset = 32*(len(args.input_files)+1)
        align_len = cur_data_offset % ATF_DATA_ALIGN_SIZE
        align_num =  cur_data_offset // ATF_DATA_ALIGN_SIZE
        if align_len > 0 :
            align_num = align_num + 1
        cur_data_offset = align_num * ATF_DATA_ALIGN_SIZE


        config_str_list = PreparseConfig(args.config_file)
        for input_file in args.input_files:
            if not os.path.isfile(input_file):
                continue

            print('ATF: Add file: %s' %input_file)
            if atf_add_file(input_file) != True:
                sys.exit(1)

        atf_gen_header()

        # write the merged property file
        with open(args.output_file, 'wb+') as f:
            f.write(atf_header)
            for af in atf_file_list:
                f.write(af.data)

        atf_add_crc(args.output_file)
    else:
        ret = extract_image(args.image_file, args.extract_out_dir)
        if ret != True:
            sys.exit(1)

if __name__ == "__main__":
    main(sys.argv)
