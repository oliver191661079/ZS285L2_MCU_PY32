#!/usr/bin/env python
#
# Copyright (c) 2017 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#

import os, sys, re, platform

def list_files(dir_path):
    file_list = [];

    for root, dirs, files in os.walk(dir_path):
        for f in files:
            if (os.path.splitext(f)[1] == ".h" or os.path.splitext(f)[1] == ".c" or os.path.splitext(f)[1] == ".prop" or os.path.splitext(f)[1] == ".conf" or os.path.splitext(f)[1] == ".xml"):
                #print f
                file_list.append(os.path.join(root, f))
    return file_list

def main():

    if len(sys.argv) != 2:
        print "Dir args is empty, Enter the path to be processed!"
        os._exit(0)

    filedir = sys.argv[1]
    print filedir.strip()

    sys_version = platform.version()
    if "Ubuntu" in sys_version:
        os.environ['syscmd'] = str("fromdos")
    else:
        print "Not find the system version!"
        os._exit(0)

    #conver CRLF to LF and GBK to UTF-8
    file_list = list_files(filedir)
    for f in file_list:
        os.environ['file'] = str(f)
        ret=os.system('enca -L zh -x UTF-8 $file ')
        #if convert error or warning, print convert files
        if ret:
            print f         
        os.system('$syscmd $file')
        

if __name__ == '__main__':
    main()

