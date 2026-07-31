BT_STACK_LIB_DIR=$(ZEPHYR_BASE)/ext/actions/bluetooth/bt_stack
BT_STACK_LIB_OBJ_DIR=$(O)/ext/actions/bluetooth/bt_stack
BT_STACK_LIB=

BT_SERVICE_LIB_OUT_DIR=$(ZEPHYR_BASE)/ext/actions/bluetooth/bt_service
BT_SERVICE_LIB_OBJ_DIR=$(O)/ext/actions/bluetooth/bt_service
BT_SERVICE_LIB=

obj-$(CONFIG_BT_MANAGER) += bt_manager/
obj-$(CONFIG_BT_SERVICE) += bt_service/
obj-$(CONFIG_BT_STACK) += bt_stack/

ifdef CONFIG_BT_SERVICE
ifneq ($(wildcard $(srctree)/ext/actions/bluetooth/bt_service/libbt_service/Makefile),)
BT_SERVICE_LIB += libbt_service
else
LINK_SYS_LIBS += $(BT_SERVICE_LIB_OUT_DIR)/libbt_service.a
endif
endif

ifdef CONFIG_BT_STACK
ifneq ($(wildcard $(srctree)/ext/actions/bluetooth/bt_stack/libbt_stack/Makefile),)
BT_STACK_LIB += libbt_stack
else
LINK_SYS_LIBS += $(BT_STACK_LIB_DIR)/libbt_stack.a
endif
endif
