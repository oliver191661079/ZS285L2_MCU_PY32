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

import re
import sys

def functions_stack_usage_analyse(assembly_file_path, elf_file_path, output_script_path):
    pattern = re.compile(r'subi\s+sp,\s+sp,\s+(\d+)')
    results = []

    try:
        with open(assembly_file_path, 'r', encoding='ISO-8859-1') as file:
            for line in file:
                match = pattern.search(line)
                if match:
                    value = int(match.group(1))
                    if value > 50:
                        address = line.split(':')[0].strip()
                        hex_address = f"0x{address}" if not address.startswith('0x') else address
                        results.append((hex_address, value))
    except UnicodeDecodeError as e:
        print(f"An error occurred while reading the file with ISO-8859-1 encoding: {e}")
        return

    with open(output_script_path, 'w', encoding='utf-8') as script_file:
        script_file.write("#!/bin/bash\n\n")
        script_file.write("echo \"Function Address\tSource File\tStack Usage\"\n")
        script_file.write("echo \"---------------------\t------------\t------------\"\n")
        for address, value in results:
            addr2line_command = f'addr2line -e {elf_file_path} {address}'
            script_file.write(f"{addr2line_command} -f -i && echo -e \"\\n{address}\t\t{value}\"\n")
            script_file.write("echo \"---------------------\t------------\t------------\"\n")
    print(f' script has been written to "{output_script_path}"')

def main():
    if len(sys.argv) != 4:
        print("Usage: python script.py <assembly_file_path> <elf_file_path> <output_script_path>")
        sys.exit(1)

    assembly_file_path = sys.argv[1]
    elf_file_path = sys.argv[2]
    output_script_path = sys.argv[3]

    functions_stack_usage_analyse(assembly_file_path, elf_file_path, output_script_path)

if __name__ == "__main__":
    main()