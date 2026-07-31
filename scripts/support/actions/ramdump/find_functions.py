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

import subprocess
import sys
import struct
import os

def read_binary_file_section(binary_file_path, start_addr, end_addr, read_start_addr, read_end_addr):
    with open(binary_file_path, 'rb') as file:
        file.seek(read_start_addr - start_addr)
        binary_data = file.read(read_end_addr - read_start_addr)

    return binary_data

def filter_addresses(binary_data, search_start_address, search_end_address):
    addresses = []
    for i in range(0, len(binary_data), 4):
        address = struct.unpack('<I', binary_data[i:i+4])[0]
        if search_start_address <= address <= search_end_address:
            addresses.append(address)
    return addresses

def addr2line(addresses, elf_file_path):
    for address in addresses:
        try:
            process = subprocess.Popen(['addr2line', '-f', '-e', elf_file_path, hex(address)],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            stdout, stderr = process.communicate()
            if process.returncode == 0:
                print(f'Address: {hex(address)} -> Function: {stdout.decode().strip()}')
            else:
                print(f'Error converting address {hex(address)}: {stderr.decode().strip()}')
        except Exception as e:
            print(f'Error: {e}')

def main(binary_file_path, binary_start_address, binary_end_address, stack_start_address, stack_end_address, search_start_address, search_end_address, elf_file_path):
    binary_data = read_binary_file_section(binary_file_path, binary_start_address, binary_end_address, stack_start_address, stack_end_address)
    addresses = filter_addresses(binary_data, search_start_address, search_end_address)
    addr2line(addresses, elf_file_path)

if __name__ == '__main__':
    if len(sys.argv) != 8:
        print("Usage: python find_functions.py <binary_file> <binary_start_address> <stack_start_address>  <stack_end_address> <search_start_address> <search_end_address> <elf_file>")
        sys.exit(1)

    binary_file_path = sys.argv[1]
    binary_start_address = int(sys.argv[2], 0)
    binary_file_length = int(os.path.getsize(binary_file_path))
    binary_end_address = binary_start_address + binary_file_length
    stack_start_address = int(sys.argv[3], 0)
    stack_end_address = int(sys.argv[4], 0)
    search_start_address = int(sys.argv[5], 0)
    search_end_address = int(sys.argv[6], 0)
    elf_file_path = sys.argv[7]

    if(stack_start_address < binary_start_address):
        print("err:stack_start_address(0x%x) < binary_start_address(0x%x)" %(stack_start_address, binary_start_address))
        sys.exit(1)

    if(stack_end_address > binary_end_address):
        print("err:stack_end_address(0x%x) > binary_end_address(0x%x)" %(stack_end_address, binary_end_address))
        sys.exit(1)

    main(binary_file_path, binary_start_address, binary_end_address, stack_start_address, stack_end_address, search_start_address, search_end_address, elf_file_path)