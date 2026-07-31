#!/bin/bash -e
#
# Copyright (c) 2017 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#

./fileformat.py ../../../arch/csky
./fileformat.py ../../../boards/csky
./fileformat.py ../../../drivers
./fileformat.py ../../../ext/lib/actions/include
./fileformat.py ../../../ext/lib/actions/libabt
./fileformat.py ../../../ext/lib/actions/libal
./fileformat.py ../../../ext/lib/actions/libapp
./fileformat.py ../../../ext/lib/actions/libota
./fileformat.py ../../../ext/lib/actions/porting
./fileformat.py ../../../ext/lib/actions/private/libsystem
./fileformat.py ../../../ext/lib/actions/private/libbt_drv
./fileformat.py ../../../ext/lib/actions/private/libbt_engine
./fileformat.py ../../../ext/lib/actions/private/libota
./fileformat.py ../../../ext/lib/actions/private/libmem_manager_rom
./fileformat.py ../../../ext/lib/actions/system
./fileformat.py ../../../ext/lib/actions/utils
./fileformat.py ../../../include
./fileformat.py ../../../kernel
./fileformat.py ../../../lib
./fileformat.py ../../../misc
./fileformat.py ../../../samples/bt_box
./fileformat.py ../../../subsys
./fileformat.py ../../../tests/actions_test
