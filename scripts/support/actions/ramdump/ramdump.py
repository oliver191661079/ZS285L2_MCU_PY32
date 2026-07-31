#!/usr/bin/env python3
###########################################################################
#
# Copyright (c) 2024 Actions Semiconductor Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
# either express or implied. See the License for the specific
# language governing permissions and limitations under the License.
#
import os
import sys
import time
import argparse
import struct
#import lz4.block
import subprocess
from hexdump import hexdump
import json

'''
/* 1. ramdump structure */
+===========+===============+===============+
| ramd_head | ramd_region 1 | ramd_region 2 | ...
+===========+===============+===============+

/* 2. ramdump region */
+=============+==============+==============+
| region_head | lz4 stream 1 | lz4 stream 2 | ...
+=============+==============+==============+

/* 3. lz4 stream */
+==========+===========+
| lz4_head | lz4 block |
+==========+===========+

/* ramd header */
typedef struct ramd_head {
	uint32_t magic;     /* magic (file format) */
	uint32_t version;   /* file version */
	uint32_t img_sz;    /* Size of image body (bytes) */
	uint32_t org_sz;    /* Size of origin data (bytes) */
	uint32_t esf_addr;
	uint32_t current_thread;
	uint8_t target_type;
	uint8_t reserved[7];
} ramd_head_t;

/* ramd region */
typedef struct ramd_region {
	uint32_t mem_addr;  /* memory address */
	uint32_t mem_sz;    /* memory size */
	uint32_t img_off;   /* image offset */
	uint32_t img_sz;    /* image size */
} ramd_region_t;

/* lz4 header */
typedef struct lz4_head {
	uint32_t magic;
	uint32_t hdr_size;   /* Size of image header (bytes) */
	uint32_t img_size;   /* Size of image body (bytes) */
	uint32_t org_size;   /* Size of origin data (bytes) */
}lz4_head_t;
'''

RAMD_MAGIC = 0x444d4152  #RAMD
LZ4_MAGIC = 0x20345a4c   #LZ4
RAMD_HDR_SIZE = 32
REGION_HDR_SIZE = 16
LZ4_HDR_SIZE = 16
lZ4_MAX_RAW = 0x1000000

def fastlz2_decompress(input_data, op, op_limit):
    ip = memoryview(input_data).cast('B')
    ip_limit = len(ip)
    ip_bound = ip_limit - 2
    ctrl = ip[0] & 31
    ip_pos = 1
    op_pos = 0
    MAX_L2_DISTANCE = 8191

    def bound_check(condition, message):
        if not condition:
            raise ValueError(message)

    try:
        while True:
            if ctrl >= 32:
                len_val = (ctrl >> 5) - 1
                ofs = (ctrl & 31) << 8
                ref_pos = op_pos - ofs - 1

                if len_val == 6:
                    while (True):
                        code = ip[ip_pos]
                        ip_pos += 1
                        len_val += code
                        if code != 255:
                            break

                code = ip[ip_pos]
                ip_pos += 1
                ref_pos -= code
                len_val += 3

                if code == 255:
                    if ofs == (31 << 8):
                        ofs = ip[ip_pos] << 8
                        ip_pos += 1
                        ofs += ip[ip_pos]
                        ip_pos += 1
                        ref_pos = op_pos - ofs - MAX_L2_DISTANCE - 1

                #print("op pos %x len %x ref_pos %x ctrl %x code %x ip_pos %x next %x" %(op_limit - op_pos, len_val, ref_pos, ctrl, code,\
                # ip_pos, ip[ip_pos]))

                bound_check(op_pos + len_val <= op_limit, "Output buffer overflow")
                bound_check(ref_pos >= 0, "Reference before output buffer start")
                idx = 0
                while(idx < len_val):
                    op[op_pos + idx] = op[ref_pos + idx]
                    idx += 1
                #op[op_pos:op_pos + len_val] = op[ref_pos:ref_pos + len_val]
                op_pos += len_val
            else:
                ctrl += 1
                bound_check(op_pos + ctrl <= op_limit, "Output1 buffer overflow")
                bound_check(ip_pos + ctrl <= ip_limit, "Input1 buffer overflow")
                op[op_pos:op_pos + ctrl] = ip[ip_pos:ip_pos + ctrl]
                ip_pos += ctrl
                op_pos += ctrl

                #if ip_pos < ip_limit:
                #    print("ctrl %x ip_pos %x next %x" %(ctrl, ip_pos, ip[ip_pos]))
                #else:
                #    print("ctrl %x ip_pos %x" %(ctrl, ip_pos))


            if ip_pos >= ip_limit:
                break

            ctrl = ip[ip_pos]
            ip_pos += 1

    except ValueError as e:
        print(f"Decompression error: {e}")
        return None

    #print("decompress len %x" %(op_pos))
    return op[:op_pos]

def ramd_ozone_cmd(cmd_file, addr_list):
    if len(addr_list) <= 0:
        return
    with open(cmd_file, 'w') as f1:
        f1.write('void LoadRam (void) {\n')
        for addr in addr_list:
            f1.write('  Target.LoadMemory("0x%08x.bin",0x%08x);\n'%(addr,addr))
        f1.write('}\n')

def ramd_enc_seg(raw_dir, output, seg_size):
    if not os.path.exists(raw_dir):
        return (0,0)

    ramd_org_sz = 0
    ramd_img_data = bytes()
    region_addr_list = []

    for name in os.listdir(raw_dir):
        raw_file = os.path.join(raw_dir,name)
        [base, ext] = os.path.splitext(name)
        if ext != ".bin":
            continue
        region_addr = int(base,16)
        region_addr_list.append(region_addr)
        region_sz = 0
        region_img_data = bytes()

        with open(raw_file, 'rb') as f1:
            # region data
            while True:
                raw_data = f1.read(seg_size)
                region_sz += len(raw_data)
                if len(raw_data) == 0:
                    break
                # lz4 data
                lz4_data = lz4.block.compress(raw_data,mode='high_compression',compression=3,store_size=False)
                # lz4 header
                hdr_data = struct.pack("<IIII", LZ4_MAGIC, LZ4_HDR_SIZE, len(lz4_data), len(raw_data))
                #print("        [lz4] img_sz=%d, org_sz=%d" %(len(lz4_data), len(raw_data)))
                region_img_data += hdr_data + lz4_data
                fill_len = (4 - len(lz4_data) % 4) % 4
                if fill_len > 0:
                    region_img_data += b'0' * fill_len

            # region header
            region_hdr_data = struct.pack("<IIII", region_addr, region_sz, 0, len(region_img_data))
            print("    [region] mem_addr=0x%08x, mem_sz=%d, img_sz=%d" %(region_addr, region_sz, len(region_img_data)))
            # ramd data
            ramd_img_data += region_hdr_data + region_img_data
            ramd_org_sz += region_sz

    # ramd header
    ramd_hdr_data = struct.pack("<IIII", RAMD_MAGIC, 0, len(ramd_img_data), ramd_org_sz)
    print("[ramdump] org_sz=%d, img_sz=%d" %(ramd_org_sz, len(ramd_img_data)))

    # ozone script
    ramd_ozone_cmd(os.path.join(raw_dir,"ozone.txt"), region_addr_list)

    with open(output, 'wb') as f2:
        f2.write(ramd_hdr_data)
        f2.write(ramd_img_data)
        return (ramd_org_sz, f2.tell())

def decompress_data(compressed_data, decompree_len):
    start_time = time.time()
    try:
        #print("data length %x size %x\n" %(len(compressed_data), decompree_len))

        #hexdump(compressed_data)
        #print(compressed_data)
        #compressed_data = b'/\x00\x00\x00\x04This  \x02\x1fsome data to be compressed with \x06fastlz.'

        #print(type(compressed_data))

        # 解压缩当前块的数据
        op = bytearray(32768)
        #hexdump(compressed_data)
        decompressed_data = fastlz2_decompress(compressed_data, op, 32768)

        end_time = time.time()
        elapsed_time = end_time - start_time
        print(f"Decompression completed in {elapsed_time:.6f} seconds.")

        return decompressed_data

    except fastlz.FastlzError as e:
        print(f"Decompression failed: {e}")
        return None

def ramd_dec_seg(ramd_file, output, json_file):
    if not os.path.exists(ramd_file):
        return (0,0)
    if not os.path.exists(output):
        os.mkdir(output)

    with open(ramd_file, 'rb') as f1:
        out_sz = 0
        region_addr_list = []
        binaries_dict = {}
        # read ramd header
        ramd_data = f1.read(RAMD_HDR_SIZE)
        if len(ramd_data) < RAMD_HDR_SIZE:
            return (0,0)
        ramd_magic, ramd_ver, ramd_img_sz, ramd_org_sz, esf_addr, current_thread, target_type, reserved = struct.unpack("<IIIIIIII", ramd_data)
        if ramd_magic != RAMD_MAGIC:
            print('error file magic 0x%x expected 0x%x\n' %(ramd_magic, RAMD_MAGIC))
            return (0, 0)
        print("[ramdump] org_sz=0x%x, img_sz=0x%x esf_addr=0x%x" %(ramd_org_sz,ramd_img_sz, esf_addr))

        while ramd_img_sz > 0:
            # read region header
            region_data = f1.read(REGION_HDR_SIZE)
            ramd_img_sz -= len(region_data)
            if len(region_data) < REGION_HDR_SIZE:
                break
            region_addr, region_sz, region_img_off, region_img_sz = struct.unpack("<IIII", region_data)
            ramd_img_sz -= region_img_sz
            print("    [region] mem_addr=0x%08x, mem_sz=0x%x, img_sz=0x%x" %(region_addr, region_sz, region_img_sz))
            region_addr_list.append(region_addr)
            raw_file = os.path.join(output, "0x%08x.bin"%region_addr)

            with open(raw_file, 'wb') as f2:
                while region_img_sz > 0:
                    # read lz4 header
                    hdr_len = min(region_img_sz, LZ4_HDR_SIZE)
                    lz4_data = f1.read(hdr_len)
                    region_img_sz -= len(lz4_data)
                    #print(lz4_data)
                    if len(lz4_data) < LZ4_HDR_SIZE:
                        break
                    lz4_magic, lz4_hdr_sz, lz4_img_sz, lz4_org_sz = struct.unpack("<IIII", lz4_data)
                    if lz4_magic != LZ4_MAGIC:
                        print('error seg magic 0x%x expected 0x%x\n file offset 0x%x' %(lz4_magic, LZ4_MAGIC, ))
                        return (0, 0)
                    print("        [lz4] img_sz=%x, org_sz=%x" %(lz4_img_sz, lz4_org_sz))
                    # read lz4 data
                    lz4_data = f1.read(lz4_img_sz)
                    region_img_sz -= len(lz4_data)
                    raw_data = decompress_data(lz4_data, lz4_org_sz)
                    if len(raw_data) != lz4_org_sz:
                        print('error size data size %x expected %x\n' %(len(raw_data), lz4_org_sz))
                        return (0, 0)
                    f2.write(raw_data)
                    # read fill data
                    fill_len = (4 - len(lz4_data) % 4) % 4
                    if fill_len > 0:
                        lz4_data = f1.read(fill_len)
                        region_img_sz -= len(lz4_data)
                out_sz += f2.tell()

                binaries_dict[os.path.join("./", raw_file)] = region_addr
                #print("%s connect %s to load memory %x from %s\n" %(gdb_path, target_path, region_addr, raw_file))
                #restore_binary_to_memory(gdb_path, target_path, os.path.join("./", raw_file), region_addr)

        with open(json_file, 'w') as f:
            json.dump(binaries_dict, f)
            f.write('\n')
            f.write(hex(esf_addr))

        return (f1.tell(), out_sz)

def test_compress():
    test_str = [0x24, 0x54, 0X68, 0X69, 0X73, 0X20, 0X20, 0X02, 0X1f, 0X73, 0X6f, 0X6d, 0X65, 0X20, 0X64, 0X61, 0X74, 0X61, 0X20, 0X74, 0X6f, 0X20, 0X62, 0X65, 0X20, 0X63, 0X6f, 0X6d, 0X70, 0X72, 0X65, 0X73, 0X73, 0X65, 0X64, 0X20, 0X77, 0X69, 0X74, 0X68, 0X20, 0X06, 0X66, 0X61, 0X73, 0X74, 0X6c, 0X7a, 0X2e]
    test_str = bytes(test_str)
    hexdump(test_str)
    op = bytearray(32768)
    a = fastlz2_decompress(test_str, op, 32768)
    hexdump(a)

def main(argv):
    parser = argparse.ArgumentParser(
        description='ramdump compress or decompress',
    )
    parser.add_argument('-c', dest = 'raw_dir')
    parser.add_argument('-d', dest = 'ramd_file')
    parser.add_argument('-o', dest = 'output')
    parser.add_argument('-s', dest = 'seg_size')
    parser.add_argument('-j', dest = 'json_file')
    args = parser.parse_args()

    if not args.output:
        parser.print_help()
        sys.exit(1)

    if not args.seg_size:
        args.seg_size = "32768"

    #test_compress()

    if args.raw_dir:
        print('ramdump compress: %s -> %s' %(args.raw_dir,args.output))
        ret = ramd_enc_seg(args.raw_dir, args.output, int(args.seg_size))
        print('size: %d -> %d (%.1f%%)' %(ret[0],ret[1],ret[1]*100/ret[0]))
    elif args.ramd_file:
        print('ramdump decompress: %s -> %s' %(args.ramd_file,args.output))
        ret = ramd_dec_seg(args.ramd_file, args.output, args.json_file)
        if ret[1] > 0:
            print('size: %d -> %d (%.1f%%)' %(ret[0],ret[1],ret[1]*100/ret[0]))
        else:
            print('lz4 decompress: %s failed!' %(args.ramd_file))

    return 0

if __name__ == '__main__':
    main(sys.argv[1:])
