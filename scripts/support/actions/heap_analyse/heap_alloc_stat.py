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
import re
import argparse
import subprocess
from collections import defaultdict
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

# 正则表达式匹配malloc和free日志
malloc_pattern = re.compile(r'\[(\d+:\d+:\d+\.\d+)\].*?malloc ptr (\w+) (\d+) caller ([\w\s]+)')
free_pattern = re.compile(r'\[(\d+:\d+:\d+\.\d+)\].*?free ptr (\w+) (\d+) caller ([\w\s]+)')
malloc_stat_pattern = re.compile(r'(\d+:\d+:\d+\.\d+) malloc (\w+) (\d+) ([\w\s]+)')

# 用于存储malloc的记录
malloc_records = []

# 用于存储已释放的内存
released_memory = []

# 用于存储相同caller和size的malloc次数
malloc_count = defaultdict(int)

# 读取日志文件
def analyze_log_records(filename):
    with open(filename, 'r') as file:
        for line in file:
            # 匹配malloc日志
            malloc_match = malloc_pattern.match(line)
            if malloc_match:
                timestamp, ptr, size, callers = malloc_match.groups()
                callers = callers.split()
                malloc_records.append((timestamp, 'malloc', ptr, int(size), callers))

            # 匹配free日志
            free_match = free_pattern.match(line)
            if free_match:
                timestamp, ptr, size, callers = free_match.groups()
                #print('free search ptr %s size %s' %(ptr, size))
                callers = callers.split()
                for malloc in malloc_records:
                    if malloc[2] == ptr and malloc[3] == int(size):
                        released_memory.append((malloc[0], malloc[1], malloc[2], malloc[3], malloc[4], timestamp, 'free   ', ptr, int(size), callers))
                        malloc_records.remove(malloc)
                        break

    # 未释放的内存
    unreleased_memory = [(record[0], record[1], record[2], record[3], record[4]) for record in malloc_records]

    return unreleased_memory, released_memory

def get_symbol_info(elf_file, address):
    command = ['addr2line', '-f', '-C', '-e', elf_file, address]
    try:
        result = subprocess.check_output(command, stderr=subprocess.STDOUT)
        output = result.decode().strip()
        lines = output.split('\n')
        function_name = lines[0].strip() if lines else None
        return function_name
    except subprocess.CalledProcessError as e:
        print(f"Error executing addr2line: {e.output.decode()}")
        return None

def parse_mpool(log_lines, output_file):
    mpool_summary = {'free_pages': 0, 'total_pages': 0, 'used_memory': 0, 'free_memory': 0}
    mpool_pattern = re.compile(r'mpool: (\d+)\(free\)\/(\d+)\(total\)')
    total_pattern = re.compile(r'total\s+(\d+)')
    used_pattern = re.compile(r'used\s+(\d+)\s+\((\d+) %\)')
    free_pattern = re.compile(r'free\s+(\d+)\s+\((\d+) %\)')

    for line in log_lines:
        if 'mpool:' in line:
            mpool_match = mpool_pattern.search(line)
            if mpool_match:
                free, total = mpool_match.groups()
                mpool_summary['free_pages'] = int(free)
                mpool_summary['total_pages'] = int(total)

            for i in range(1, 4):
                if i < len(log_lines):
                    next_line = log_lines[log_lines.index(line) + i]
                    #print(next_line)
                    if "total" in next_line:
                        total_match = total_pattern.search(next_line)
                        if total_match:
                            mpool_summary['total_memory'] = int(total_match.group(1))
                    elif "used" in next_line:
                        used_match = used_pattern.search(next_line)
                        if used_match:
                            mpool_summary['used_memory'] = int(used_match.group(1))
                            mpool_summary['used_memory_percentage'] = int(used_match.group(2))
                    elif "free" in next_line:
                        free_match = free_pattern.search(next_line)
                        if free_match:
                            mpool_summary['free_memory'] = int(free_match.group(1))
                            mpool_summary['free_memory_percentage'] = int(free_match.group(2))
                else:
                    break

    output_file.write("Mpool Summary:\n")
    output_file.write(f"Free Pages: {mpool_summary['free_pages']}, Total Pages: {mpool_summary['total_pages']}\n")
    output_file.write(f"Total Memory: {mpool_summary['total_memory']}, Used Memory: {mpool_summary['used_memory']} ({mpool_summary['used_memory_percentage']}%)\n")
    output_file.write(f"Free Memory: {mpool_summary['free_memory']} ({mpool_summary['free_memory_percentage']}%)\n\n")

def parse_malloc_details(log_lines, output_file, elf_file):
    malloc_details = []
    thread_memory = {}
    capture_malloc = False
    total_requested_size = 0
    total_allocated_size = 0

    for line in log_lines:
        if "address 	size(space thread)	caller" in line or "malloc detail:" in line:
            capture_malloc = True
            continue
        if capture_malloc:
            if "malloc detail end" in line:
                capture_malloc = False
                continue
            parts = line.split()
            #print(parts, len(parts))
            if len(parts) >= 6 and "0x" in parts[1]:
                address = parts[1]
                requested_size = int(parts[2].split('(')[0])
                actual_size = int(parts[3])
                thread_str = parts[4].strip('()')
                thread = int(thread_str)
                caller = parts[5].strip('()')

                if requested_size != actual_size:
                    symbol = get_symbol_info(elf_file, caller)
                    if symbol:
                        caller = caller + '_' + symbol
                else:
                    thread = 0

                malloc_details.append({
                    'address': address,
                    'requested_size': requested_size,
                    'actual_size': actual_size,
                    'thread': thread,
                    'caller': caller
                })

                if requested_size != actual_size:
                    thread_memory[thread] = thread_memory.get(thread, 0) + requested_size
                else:
                    thread_memory[0] = thread_memory.get(thread, 0) + requested_size

                total_requested_size = total_requested_size + requested_size

                total_allocated_size = total_allocated_size + actual_size

    output_file.write("Malloc Details:\n")
    for detail in malloc_details:
        output_file.write(f"Address: {detail['address']}, Requested Size: {detail['requested_size']}, "
                          f"Actual Size: {detail['actual_size']}, Thread: {detail['thread']}, "
                          f"Caller: {detail['caller']}\n")

    output_file.write("\nThread Memory Usage:\n")
    for thread, memory in thread_memory.items():
        output_file.write(f"Thread {thread}: {memory} bytes\n")

    output_file.write(f"\nTotal Thread Memory Usage:{total_requested_size}/{total_allocated_size}\n")

def mpool_malloc_analyze_log(filename, output_filename, elf_file):
    with open(filename, 'r') as file:
        log_lines = file.readlines()

    with open(output_filename, 'a') as output_file:
        parse_mpool(log_lines, output_file)
        parse_malloc_details(log_lines, output_file, elf_file)

def main(argv=None):
    parser = argparse.ArgumentParser(description='Analyze heap allocation and free logs')
    parser.add_argument('-i', dest='log_file', required=True, help='Input log file')
    parser.add_argument('-e', dest='elf_file', required=True, help='ELF file path')
    args = parser.parse_args()

    unreleased_memory, released_memory = analyze_log_records(args.log_file)

    release_memory_log_file_name = os.path.splitext(args.log_file)[0] + '_release_memory.log'
    unrelease_memory_log_file_name = os.path.splitext(args.log_file)[0] + '_unrelease_memory.log'


    # 写入已释放的内存到log文件
    with open(release_memory_log_file_name, 'w') as file:
        for record in released_memory:
            malloc_callers = ' '.join(record[4])
            free_callers = ' '.join(record[9])
            file.write(f"{record[0]} {record[1]} {record[2]} {record[3]} {malloc_callers}\n")
            file.write(f"{record[5]} {record[6]} {record[7]} {record[8]} {free_callers}\n")

    print("parse %s data " %(release_memory_log_file_name))

    # 写入未释放的内存到log文件
    with open(unrelease_memory_log_file_name, 'w') as file:
        for record in unreleased_memory:
            callers = ' '.join(record[4])
            for caller in record[4]:
                symbol = get_symbol_info(args.elf_file, caller)
                if symbol:
                    callers = callers.replace(caller, symbol)
            file.write(f"{record[0]} {record[1]} {record[2]} {record[3]} {callers}\n")
            print("parse %s data " %(unrelease_memory_log_file_name))

    with open(release_memory_log_file_name, 'r') as file:
        for line in file:
            match = malloc_stat_pattern.match(line)
            if match:
                timestamp, ptr, size, callers = match.groups()
                # 使用caller信息和size作为键来统计malloc次数
                key = (callers, int(size))
                malloc_count[key] += 1

    # 写入统计结果到released_memory.log文件的最后
    with open(release_memory_log_file_name, 'a') as file:
        file.write('\n\nMalloc Count Statistics:\n')
        for (callers, size), count in sorted(malloc_count.items(), key=lambda item: item[1], reverse=True):
            for caller in callers.split():
                symbol = get_symbol_info(args.elf_file, caller)
                if symbol:
                    callers = callers.replace(caller, symbol)
            file.write(f"malloc Size: {size}, Count: {count} Caller: {callers}\n")
            print("parse stat memory data ")

    mpool_malloc_analyze_log(args.log_file, unrelease_memory_log_file_name, args.elf_file)

if __name__ == '__main__':
    main()