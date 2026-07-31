# Copyright (c) 2020 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0

SET(LIB_NAME k1_mp3_p)
SET(ZEPHYR_CURRENT_LIBRARY ${LIB_NAME})
SET(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_LIST_DIR}/../")

zephyr_include_directories(./)

zephyr_library_named(${LIB_NAME})

zephyr_library_sources(
	my_mp3_parser.c
)

target_link_libraries(${LIB_NAME} INTERFACE zephyr_interface)
