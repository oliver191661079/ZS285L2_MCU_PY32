#!/usr/bin/env python3
#
# Copyright (c) 2020 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import binascii
import sys
import zlib
import re

RAMDUMP_PREFIX_STR = "#DA:"

RAMDUMP_BEGIN_STR = RAMDUMP_PREFIX_STR + "BEGIN#"
RAMDUMP_CRC32_STR = RAMDUMP_PREFIX_STR + "CRC32#"
RAMDUMP_BLOCK_STR = RAMDUMP_PREFIX_STR + "BLOCK#"
RAMDUMP_END_STR = RAMDUMP_PREFIX_STR + "END#"
RAMDUMP_ERROR_STR = RAMDUMP_PREFIX_STR + "ERROR CANNOT DUMP#"


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument("infile", help="Serial Log File")
    parser.add_argument("outfile", help="Output file for use with ramdump")
    #parser.add_argument("outfile_backup", help="Output backup file for use with compare with output file")

    return parser.parse_args()

def preprocess_file(infile_path):
    with open(infile_path, "r", encoding='utf-8') as infile:
        lines = infile.readlines()

    # Preprocess lines to merge lines without timestamp
    processed_lines = []
    for i, line in enumerate(lines):
        if line.startswith("["):
            # If the current line starts with a timestamp, check if there is a previous line
            if processed_lines:
                processed_lines.append(line)
            else:
                # This is the first line in the file
                processed_lines.append(line)
        else:
            # If the current line does not start with a timestamp
            if processed_lines:
                # Merge the current line with the previous line after removing newline characters from the previous line
                processed_lines[-1] = processed_lines[-1].rstrip('\r\n') + line
            else:
                # This is the first line in the file
                processed_lines.append(line)

    # Write processed lines to a temporary file
    tmpfile_path = infile_path + '.tmp'
    with open(tmpfile_path, 'w', encoding='utf-8') as tmpfile:
        tmpfile.writelines(processed_lines)

    return tmpfile_path


def save_file(infile_path, outfile, line_number=0):
    has_begin = False
    has_end = False
    has_error = False
    go_parse_line = False
    skip_line = False
    bytes_written = 0
    inner_line_num = 0
    write_crc32 = 0
    log_crc32 = 0
    write_block_crc32 = 0
    log_block_crc32 = 0

    infile = open(infile_path, "r")

    if line_number:
        skip_line = True

    for line in infile.readlines():
        if(skip_line):
            #print('cur line %d search %d' %(inner_line_num, line_number))
            if(inner_line_num < line_number):
                inner_line_num += 1
                continue
            else:
                skip_line = False

        line_number += 1
        #print(line)
        #if line.find(RAMDUMP_BEGIN_STR) >= 0:
        if RAMDUMP_BEGIN_STR in line:
            # Found "BEGIN#" - beginning of log
            has_begin = True
            go_parse_line = True
            continue

        if RAMDUMP_BLOCK_STR in line:
            pattern = r"#DA:BLOCK#(\d+)#0x([\dA-Fa-f]+)"
            match = re.search(pattern, line)
            #print(line)
            #print(match.group(1))
            block_number = int(match.group(1))
            crc32_str = match.group(2)
            log_block_crc32 = int(crc32_str, 16)
            if(log_block_crc32 != write_block_crc32):
                print(f"ERROR:block {block_number} cal crc32 {write_block_crc32:8x} log crc32 {log_block_crc32:8x}")
            write_block_crc32 = 0
            continue

        if RAMDUMP_CRC32_STR in line:
            # Found "CRC32#" - crc32 of log
            crc32_str = line.split(RAMDUMP_CRC32_STR)[1]
            log_crc32 = int(crc32_str, 16)
            continue

        #if line.find(RAMDUMP_END_STR) >= 0:
        if RAMDUMP_END_STR in line:
            # Found "END#" - end of log
            has_end = True
            go_parse_line = False
            break

        if line.find(RAMDUMP_ERROR_STR) >= 0:
            # Error was encountered during dumping:
            # log is not usable
            has_error = True
            go_parse_line = False
            break

        if not go_parse_line:
            continue

        prefix_idx = line.find(RAMDUMP_PREFIX_STR)

        if prefix_idx < 0:
            continue

        prefix_idx += len(RAMDUMP_PREFIX_STR)
        hex_str = line[prefix_idx:].strip()

        try:
            binary_data = binascii.unhexlify(hex_str)
            write_crc32 = zlib.crc32(binary_data, write_crc32) & 0xffffffff
            write_block_crc32 = zlib.crc32(binary_data, write_block_crc32) & 0xffffffff
            outfile.write(binary_data)
            bytes_written += len(binary_data)
        except binascii.Error as e:
            print(f"ERROR on line {line_number}: Unable to convert hex string to binary - {hex_str}")
            print(f"Error: {e}")
            return 0, line_number

    if not has_begin:
        print("ERROR: Beginning of log not found {line_number}!")
    elif not has_end:
        print("WARN: End of log not found! Is log complete? {line_number}")
    elif has_error:
        print(f"ERROR: log has error at line {line_number}.")
        return 0, line_number
    else:
        print(f"Bytes written {bytes_written} parse line:{line_number}")
        if(log_crc32 != write_crc32):
            print(f"ERROR:cal crc32 {write_crc32:8x} log crc32 {log_crc32:8x}")
    return bytes_written, line_number

def main():
    args = parse_args()
    infile_path = args.infile
    try:
        tmpfile_path = preprocess_file(infile_path)

        with open(args.outfile, "wb") as outfile:


            print(f"Input file {infile_path}")
            print(f"Output file {args.outfile}")

            line_number = 0

            save_len, line_number = save_file(tmpfile_path, outfile, line_number)
            if save_len > 0:
                print("ramdump serial log convert ok!")
            else:
                print(f"ERROR first Bytes written {save_len}")
    except IOError as e:
        print(f"I/O error({e.errno}): {e.strerror}")



if __name__ == "__main__":
    main()
