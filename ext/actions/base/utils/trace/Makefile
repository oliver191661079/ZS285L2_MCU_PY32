CUR_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

OBJ_S := $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/src/*.s))
OBJ_C := $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/src/*.c))

OBJ_S := $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/src/debug/*.s))
OBJ_C += $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/src/debug/*.c))

ifdef CONFIG_ACTIONS_TRACE
ZEPHYRINCLUDE += -I$(CUR_DIR)/include
ZEPHYRINCLUDE += -I$(CUR_DIR)/../cbuf/include
ZEPHYRINCLUDE += -I$(CUR_DIR)/../../../../fs/fat/include
obj-y := $(OBJ_C:.c=.o) $(OBJ_S:.S=.o)
endif

