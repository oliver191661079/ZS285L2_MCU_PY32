# Copyright (c) 2020 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(.)

zephyr_library_sources(
    input_manager.c
	input_keypad.c
	input_pointer.c
	input_encoder.c
)
