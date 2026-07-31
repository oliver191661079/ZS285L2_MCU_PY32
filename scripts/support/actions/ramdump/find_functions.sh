#!/bin/bash -e
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


#该脚本用于调用ramdump.py脚本实现对ramdump数据的解析，并调用csky-gdb工具实现将ramdump数据加载到mcu的内存区域并设置相关通用寄存器
#首先请确保小机已经处于开机状态，jtag使能，使用jtag调试器已经让cpu处于halt状态，需要注意CDS工具不能连接，必须断开与CDS调试工具的连接
#等数据加载完毕，脚本运行结束后可以使用CDS连接小机查看死机现场

#参数 -d ramdump文件路径，一般为文件名
#参数 -o ramdump文件解压缩后的路径，会根据文件的内存地址自动创建相关文件名
#参数 -gdb_path 配置需要运行的gdb路径，当前为csky-abiv2-elf-gdb文件的路径，需要根据CDS的安装路径进行修改
#参数 -host_server 配置需要运行的gdb server名称，一般为localhost
#参数 -host_port 配置需要运行的gdb server端口号，一般debugserver端口号就是1020，如果gdbserver修改了这个端口号，这里也需要修改

#This script is used to call the ramdump.py script to parse the ramdump data, and invoke the csky-gdb tool to load the ramdump data
#into the MCU's memory area and set relevant general registers.
#First, please ensure that the microcomputer is powered on, the JTAG is enabled, and the CPU is in a halt state using the JTAG debugger.
#It is important to note that the CDS tool must not be connected, and the connection with the CDS debugging tool must be disconnected.
#After the data loading is completed and the script runs to the end, you can connect to the microcomputer using CDS to view the crash scene.


#Parameter -d represents the path of the ramdump file, which is typically the filename.
#Parameter -o indicates the path where the ramdump file will be uncompressed and saved.
#		      It automatically creates relevant filenames based on the memory address of the file.
#Parameter -gdb_path configures the path of the gdb that needs to be run. Currently, it is the path of the csky-abiv2-elf-gdb file,
#                    which needs to be modified according to the installation path of CDS.
#Parameter -host_server   configures the name of the gdb server that needs to be run, which is typically localhost.
#Parameter -host_port configures the port number of the gdb server that needs to be run.
#                    Typically, the debugserver port number is 1020. If gdbserver modifies this port number, it also needs to be modified here.

python ./find_functions.py ./ramdumpdir/0x0004e000.bin 0x4e000 0x4e000 0x4e600 0x1000000 0x10dddbc ../../../../samples/bt_speaker/outdir/ats2875h_evb_flip7/zephyr.elf
