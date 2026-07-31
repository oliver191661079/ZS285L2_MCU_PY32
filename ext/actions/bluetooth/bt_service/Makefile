ZEPHYRINCLUDE += -I$(srctree)/ext/actions/bluetooth/bt_service/include
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/bluetooth/bt_stack/src/inc
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/bluetooth/bt_service

obj-y += btsrv_main.o
obj-y += core/
obj-y += profile/
obj-y += btif/

obj-$(CONFIG_GFP_PROFILE) += gfp/

ifneq ($(wildcard $(srctree)/ext/actions/bluetooth/bt_service/libbt_service/Makefile),)
obj-y += libbt_service/
endif
