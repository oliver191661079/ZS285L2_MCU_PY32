CUR_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

OBJ_S := $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/src/*.s))
OBJ_C := $(patsubst $(CUR_DIR)%,%,$(wildcard $(CUR_DIR)/src/*.c))

ZEPHYRINCLUDE += -I$(CUR_DIR)/include
obj-y := $(OBJ_C:.c=.o) $(OBJ_S:.S=.o)

