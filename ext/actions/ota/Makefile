# Copyright (c) 2020 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0

OTA_LIB_OUT_DIR=$(ZEPHYR_BASE)/ext/actions/ota
OTA_LIB_OBJ_DIR=$(O)/ext/actions/ota
OTA_LIB=

obj-$(CONFIG_OTA_BACKEND_BLUETOOTH) +=  ota_backend_bt.o
obj-$(CONFIG_OTA_BACKEND_DISK) +=  ota_backend_disk.o
obj-$(CONFIG_OTA_BACKEND_TEMP_PART) +=  ota_backend_temp_part.o
obj-$(CONFIG_OTA_BACKEND_UART) += ota_backend_uart.o
obj-$(CONFIG_OTA_SELF_APP) += ota_backend_selfapp.o
obj-$(CONFIG_OTA_BACKEND_LETWS_STREAM) += ota_backend_letws_stream.o
obj-$(CONFIG_OTA_UNLOAD) +=  ota_unloading.o

obj-$(CONFIG_PROTOCOL_BASIC_SUPPORT) += ota_backend_selfapp.o

ifdef CONFIG_OTA
ifneq ($(wildcard $(srctree)/ext/actions/ota/libota/Makefile),)
obj-y += libota/
OTA_LIB += libota
else
LINK_SYS_LIBS += $(OTA_LIB_OUT_DIR)/libota.a
endif
ifneq ($(wildcard $(srctree)/ext/actions/ota/minlzma/Makefile),)
obj-y += minlzma/
OTA_LIB += libminlzma
else
LINK_SYS_LIBS += $(OTA_LIB_OUT_DIR)/libminlzma.a
endif
endif

