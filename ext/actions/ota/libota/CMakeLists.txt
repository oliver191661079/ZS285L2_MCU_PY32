# Copyright (c) 2020 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0

SET(LIB_NAME ota)
SET(ZEPHYR_CURRENT_LIBRARY ${LIB_NAME})
SET(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_LIST_DIR}/../")

zephyr_include_directories(./)

zephyr_library_named(${LIB_NAME})

zephyr_library_sources_ifdef(CONFIG_OTA_UPGRADE
    ota_upgrade.c
	ota_image.c
	ota_manifest.c
	ota_storage.c
	ota_breakpoint.c
)

zephyr_library_sources_ifdef(CONFIG_OTA_FILE_PATCH
    ota_file_patch.c
	hpatch.c
)
zephyr_library_sources(
	libota_version.c
)
target_link_libraries(${LIB_NAME} INTERFACE zephyr_interface)
