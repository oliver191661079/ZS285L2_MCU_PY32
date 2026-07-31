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
import subprocess
import time
import re
import json

mcu_registers_name = ["r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "epsr", "pc"]

def gdb_initialize(gdb_path, gdb_server_host, gdb_server_port, timeout=2):
    gdb_commands = [
        'target remote {}:{}'.format(gdb_server_host, gdb_server_port),
        'set confirm off',
        'set pagination off',
    ]


    gdb_process = subprocess.Popen([gdb_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)

    for cmd in gdb_commands:
        print(cmd)
        gdb_process.stdin.write(cmd + '\n')
        gdb_process.stdin.flush()

    start_time = time.time()
    response = ""
    connected = False

    # 循环读取 GDB 的输出，直到找到连接成功的信息或超时
    while not connected and (time.time() - start_time) < timeout:
        line = gdb_process.stdout.readline().strip()
        response += line + '\n'
        #print("GDB Output:", line)

        # 检查是否连接成功
        if 'Remote debugging' in line or 'Connected' in line:
            connected = True
        # 这句话表示得到小机的断点位置
        #if ' in ' in line :


        # 如果没有新的输出，稍微等待一下再检查
        if not line:
            time.sleep(0.1)

    # 检查 GDB 进程是否仍在运行
    if gdb_process.poll() is not None and not connected:
        raise Exception("GDB process terminated unexpectedly")

    # 如果没有连接成功且超时
    if not connected:
        raise Exception("GDB failed to connect within the timeout period. Last response:\n{}".format(response))

    # 如果没有错误，返回 GDB 进程对象
    return gdb_process


def gdb_write_memory(gdb_process, address, value):
    gdb_command = f'set *(unsigned int*){address} = {value}'
    #print(gdb_command)
    gdb_process.stdin.write(gdb_command + '\n')
    gdb_process.stdin.flush()


# 从MCU内存中读取数据,以32bit模式读取
def gdb_read_memory(gdb_process, address, size):
    gdb_command = f'x/{size}xw {address}\n'
    #print(gdb_command)
    gdb_process.stdin.write(gdb_command + '\n')
    gdb_process.stdin.flush()

    read_end = False
    address_str = hex(address)[2:]
    while not read_end:
        # 读取GDB的下一行输出
        line = gdb_process.stdout.readline()
        #print("GDB output:", line)
        if line:
            # 使用正则表达式检查是否找到匹配项
            match = re.search(rf'{re.escape(address_str)}:\t(0x[0-9a-fA-F]+)', line)
            if match:
                # 提取值
                value_str = match.group(1)
                value = int(value_str, 16)
                #print(f"Read memory at 0x{address_str} value 0x{value:x}\n")
                return value
        else:
            read_end = True
            break

    # 如果没有找到匹配的值，返回None
    print("No match found in GDB output\n")
    return None

def gdb_load_binary(gdb_process, binaries_dict):
    # 加载内存数据
    for binary_path, memory_address in binaries_dict.items():
        memory_address = hex(memory_address)
        gdb_command = f'restore {binary_path} binary {memory_address}'
        #print(gdb_command)
        gdb_process.stdin.write(gdb_command + '\n')
        gdb_process.stdin.flush()

        # 等待GDB的响应，这里可能需要读取多行来确保加载完成
        load_success = False
        while not load_success:
            line = gdb_process.stdout.readline().strip()
            print("load memory response:", line)
            if 'Restoring binary file' in line:
                load_success = True
                break
            elif 'Error' in line:
                raise Exception(f"Failed to load binary: {line}")

    return load_success

# 将数据写入通用寄存器和EPSR寄存器
def write_registers(gdb_process, register, value):
    gdb_process.stdin.write(f'set ${register} = {value}\n')
    gdb_process.stdin.flush()

    gdb_process.stdin.write(f'info register ${register}\n')
    gdb_process.stdin.flush()

    read_end = False
    while not read_end:
        # 读取GDB的下一行输出
        line = gdb_process.stdout.readline()
        #print("GDB output:", line)
        if line:
            # 使用正则表达式检查是否找到匹配项
            match = re.search(rf"{register}\s+(\w+)", line)
            if match:
                # 提取值
                value_str = match.group(1)
                print(f"The value of {register} is: {value_str}")
                value = int(value_str, 16)
                return value
        else:
            read_end = True
            break

    # 如果没有找到匹配的值，返回None
    print("No match found in GDB output\n")
    return None

#参数 esf_addr: ramdump头记录的GPR寄存器内存地址
# esf size：暂时未使用
# binaries_dick: ramdump得到的内存bin文件字典数据结构，包含二进制文件路径及加载地址
def hardware_setup(gdb_path, gdb_host, gdb_port, esf_addr, esf_size, binaries_dict):
    # 替换为你的csky-elfabiv2-gdb的路径
    try:
        gdb_process = gdb_initialize(gdb_path, gdb_host, gdb_port)
    except Exception as e:
        print(f"Error initializing GDB: {e}")
        exit(1)

    #写mcu寄存器防止看门狗重启
    watchdog_addr = 0xc012001c
    watchdog_value = 0x01
    try:
        gdb_write_memory(gdb_process, watchdog_addr, watchdog_value)
    except Exception as e:
        print(f"Error disabling watchdog: {e}")

    try:
        gdb_read_memory(gdb_process, watchdog_addr, 1)
    except Exception as e:
        print(f"Error read memory: {e}")

    try:
        load_success = gdb_load_binary(gdb_process, binaries_dict)
        if load_success:
            print("Binary loaded successfully.")
        else:
            print("Failed to load binary.")
    except Exception as e:
        print(f"Error loading binary: {e}")

    try:
        reg_num = len(mcu_registers_name)
        for i in range(reg_num):
            reg_value = gdb_read_memory(gdb_process, esf_addr + i * 4, 1)
            print("read memory register %s value 0x%x\n" %(mcu_registers_name[i], reg_value))
            write_registers(gdb_process, mcu_registers_name[i], reg_value)

    except Exception as e:
        print(f"Error set registers: {e}")

def main(argv):
    parser = argparse.ArgumentParser(
        description='hardware setup',
    )

    parser.add_argument('-i', dest = 'gdb_path')
    parser.add_argument('-s', dest = 'gdb_host')
    parser.add_argument('-p', dest = 'gdb_port')
    parser.add_argument('-j', dest = 'json_file')

    args = parser.parse_args()

    with open(args.json_file, 'r') as f:
        content = f.read()

    binaries_dict = json.loads(content.split('\n')[0])

    esf_addr = int(content.split('\n')[1], 16)

    print("load binary file:", binaries_dict)
    print("\nload esf addr %x\n", esf_addr)

    hardware_setup(args.gdb_path, args.gdb_host, args.gdb_port, esf_addr, 18 * 4, binaries_dict)

    return 0

if __name__ == '__main__':
    main(sys.argv[1:])
