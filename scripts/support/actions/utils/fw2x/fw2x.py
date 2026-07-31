#!/usr/bin/python

#-------------------------------------------------------------------------------
# Name:        fw2x.py
# Purpose:
#
# Author:      SRD3
#
# Created:     04-05-2015
# Copyright:   2014-2015 Actions (Zhuhai) Technology Co., Limited,
# Licence:     <your licence>
#-------------------------------------------------------------------------------

import os;
import sys;
import argparse;

from encrypt import *

def main(argv):
    parser = argparse.ArgumentParser(
        description='Randomizer encryption',
    )
    parser.add_argument('-i', dest = 'input_file')
    parser.add_argument('-o', dest = 'output_file')
    parser.add_argument('-ef', dest = 'efuse_bin')
    parser.add_argument('-b', type = int, dest = 'block_size')
    args = parser.parse_args();

    input_file = args.input_file
    output_file = args.output_file
    efuse_bin = args.efuse_bin
    block_size = args.block_size

    if (not os.path.isfile(input_file)):
        print('input file is not exist')
        sys.exit(1)

    if (not os.path.isfile(efuse_bin)):
        print('efuse_bin is not exist')
        sys.exit(1)

    if (block_size != 32) and (block_size != 34):
        print('block size %d invalid' %(block_size))
        sys.exit(1)

    sys.path.append(os.path.realpath('.'))

    try:
        ef = BaseEfuse()
        ef.encrypt_file(input_file, output_file, block_size, efuse_bin)

    except Exception as e:
        print('\033[1;31;40m')
        print('unknown exception, %s' %(e));
        print('\033[0m')
        sys.exit(2)

if __name__ == '__main__':
    main(sys.argv[1:])