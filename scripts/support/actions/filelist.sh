#!/bin/bash -e
#
# Copyright (c) 2021 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#

CODE_ROOT=$1

DIRS="${CODE_ROOT}/arch/csky"
DIRS+=" ${CODE_ROOT}/boards/csky"
DIRS+=" ${CODE_ROOT}/drivers"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/include"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/libabt"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/libal"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/libapp"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/libota"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/porting"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/private/libsystem"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/private/libbt_drv"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/private/libbt_engine"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/private/libota"
#DIRS+=" ${CODE_ROOT}/ext/lib/actions/private/libmem_manager_rom"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/system"
DIRS+=" ${CODE_ROOT}/ext/lib/actions/utils"
DIRS+=" ${CODE_ROOT}/include"
DIRS+=" ${CODE_ROOT}/kernel"
DIRS+=" ${CODE_ROOT}/lib"
DIRS+=" ${CODE_ROOT}/misc"
DIRS+=" ${CODE_ROOT}/samples/bt_box"
DIRS+=" ${CODE_ROOT}/subsys"
DIRS+=" ${CODE_ROOT}/tests/actions_test"

for dir in ${DIRS}; do
	#echo $dir
	if [ -d ${dir} ]; then
		find ${dir} -name *.c -o -name *.h
	elif [ -f ${dir} ]; then
		echo ${dir}
	fi
done

