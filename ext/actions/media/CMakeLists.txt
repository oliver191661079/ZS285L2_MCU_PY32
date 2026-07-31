# Copyright (c) 2020 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0

if (CONFIG_MEDIA)

set(ZEPHYR_CURRENT_LIBRARY acts_media)

add_library(acts_media INTERFACE)

zephyr_include_directories(
	include
)

target_include_directories(acts_media INTERFACE
	include
)

zephyr_library()

zephyr_library_sources(
    media_player.c
	media_mem.c
	media_effect_param.c
	codec_config.c
)

add_subdirectory(libal)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libmedia/CMakeLists.txt)
	add_subdirectory(libmedia)
else()
	zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR})
    zephyr_library_import(media ${CMAKE_CURRENT_LIST_DIR}/libmedia.a)
endif()

zephyr_library_link_libraries(acts_media)

zephyr_library_link_libraries_ifdef(CONFIG_FAT_FILESYSTEM_ELM ELMFAT INTERFACE)

target_link_libraries(acts_media INTERFACE zephyr_interface)

endif()



