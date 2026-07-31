SOC_LIB_OUT_DIR=$(ZEPHYR_BASE)/arch/csky/soc/actions/andesc
SOC_LIB_OBJ_DIR=$(O)/arch/csky/soc/actions/andesc/private
SOC_LIB=

ifneq ($(wildcard $(srctree)/arch/csky/soc/actions/andesc/private/librom_code/Makefile),)
obj-y += private/librom_code/
SOC_LIB += librom_code
else
LINK_SYS_LIBS += $(SOC_LIB_OUT_DIR)/librom_code.a
endif
