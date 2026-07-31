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


#该脚本用于调用ramdump.py脚本实现对ramdump数据的解析

#参数 -d ramdump文件路径，一般为文件名
#参数 -o ramdump文件解压缩后的路径，会根据文件的内存地址自动创建相关文件名
#参数 -j ramdump文件解析后生成的json文件，可以给hardware_setup.py使用，传递参数

#This script is used to call the ramdump.py script to parse the ramdump data


#Parameter -d represents the path of the ramdump file, which is typically the filename.
#Parameter -o indicates the path where the ramdump file will be uncompressed and saved.
#		      It automatically creates relevant filenames based on the memory address of the file.
#Parameter -j json file for send parameters to hardware_setup.py


python ./ramdump.py -d ramdump.bin -o ramdumpdir -j ramdump.json
