#
# Copyright (c) 2020 Actions Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Actions
#
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/bluetooth/bt_stack/src/inc
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/bluetooth/bt_stack

obj-y += src/
obj-y += common/
obj-$(CONFIG_BT_CONN) += services/

ifneq ($(wildcard $(srctree)/ext/actions/bluetooth/bt_stack/libbt_stack/Makefile),)
obj-y += libbt_stack/
endif


