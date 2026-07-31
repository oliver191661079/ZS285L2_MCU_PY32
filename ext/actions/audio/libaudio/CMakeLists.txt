# Copyright (c) 2020 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0

SET(LIB_NAME audio)
SET(ZEPHYR_CURRENT_LIBRARY ${LIB_NAME})
SET(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_LIST_DIR}/../")

zephyr_include_directories(./)

zephyr_library_named(${LIB_NAME})

target_include_directories(${LIB_NAME} INTERFACE
	.
)

zephyr_library_sources(
	bluetooth_tws_observer.c
	simple_tws_observer.c
)

target_link_libraries(${LIB_NAME} INTERFACE zephyr_interface)
