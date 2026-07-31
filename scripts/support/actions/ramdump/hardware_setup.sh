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


#该脚本用于调用hardware_setup.py脚本实现对ramdump数据的加载，并调用csky-gdb工具实现将ramdump数据加载到mcu的内存区域并设置相关通用寄存器
#首先请确保小机已经处于开机状态，jtag使能，使用jtag调试器已经让cpu处于halt状态，需要注意CDS工具不能连接，必须断开与CDS调试工具的连接
#等数据加载完毕，脚本运行结束后可以使用CDS连接小机查看死机现场

#参数 -i 配置需要运行的gdb路径，当前为csky-abiv2-elf-gdb文件的路径，需要根据CDS的安装路径进行修改
#参数 -s 配置需要运行的gdb server名称，一般为localhost
#参数 -p 配置需要运行的gdb server端口号，一般debugserver端口号就是1020，如果gdbserver修改了这个端口号，这里也需要修改

#This script is used to call the hardware_setup.py script to parse the ramdump data, and invoke the csky-gdb tool to load the ramdump data
#into the MCU's memory area and set relevant general registers.
#First, please ensure that the microcomputer is powered on, the JTAG is enabled, and the CPU is in a halt state using the JTAG debugger.
#It is important to note that the CDS tool must not be connected, and the connection with the CDS debugging tool must be disconnected.
#After the data loading is completed and the script runs to the end, you can connect to the microcomputer using CDS to view the crash scene.


#Parameter -i configures the path of the gdb that needs to be run. Currently, it is the path of the csky-abiv2-elf-gdb file,
#                    which needs to be modified according to the installation path of CDS.
#Parameter -s   configures the name of the gdb server that needs to be run, which is typically localhost.
#Parameter -p configures the port number of the gdb server that needs to be run.
#                    Typically, the debugserver port number is 1020. If gdbserver modifies this port number, it also needs to be modified here.

python ./hardware_setup.py -i '/mnt/d/C-Sky/CDS/MinGW/csky-elfabiv2-tools/bin/csky-abiv2-elf-gdb' -s 'localhost' -p 1025 -j 'ramdump.json' > hardware_setup.log
