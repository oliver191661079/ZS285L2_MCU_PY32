# Copyright (c) 2020 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0

SECUREBOOT_LIB_OUT_DIR=$(ZEPHYR_BASE)/ext/actions/secure_boot
SECUREBOOT_LIB_OBJ_DIR=$(O)/ext/actions/secure_boot
SECUREBOOT_LIB=


ifneq ($(wildcard $(srctree)/ext/actions/secure_boot/libsecure_boot/Makefile),)
obj-y += libsecure_boot/
SECUREBOOT_LIB += libsecure_boot
else
LINK_SYS_LIBS += $(SECUREBOOT_LIB_OUT_DIR)/libsecure_boot.a
endif


