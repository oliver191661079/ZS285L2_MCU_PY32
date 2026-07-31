# Copyright (c) 2020 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0

zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR})

if (CONFIG_PARSER_MP3)
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libmp3_parser/CMakeLists.txt)
	add_subdirectory(libmp3_parser)
else()
    zephyr_library_import(k1_mp3_p ${CMAKE_CURRENT_LIST_DIR}/k1_mp3_p.a)
endif()
endif()


if (CONFIG_PARSER_WAV)
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libwav_parser/CMakeLists.txt)
	add_subdirectory(libwav_parser)
else()
    zephyr_library_import(k1_wav_p ${CMAKE_CURRENT_LIST_DIR}/k1_wav_p.a)
endif()
endif()

if (CONFIG_PARSER_WMA)
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libwma_parser/CMakeLists.txt)
	add_subdirectory(libwma_parser)
else()
	zephyr_library_import(k1_wma_p ${CMAKE_CURRENT_LIST_DIR}/k1_wma_p.a)
endif()
endif()


if (CONFIG_PARSER_APE)
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libape_parser/CMakeLists.txt)
	add_subdirectory(libape_parser)
else()
	zephyr_library_import(k1_ape_p ${CMAKE_CURRENT_LIST_DIR}/k1_ape_p.a)
endif()
endif()

if (CONFIG_PARSER_FLAC)
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libflac_parser/CMakeLists.txt)
	add_subdirectory(libflac_parser)
else()
	zephyr_library_import(k1_flac_p ${CMAKE_CURRENT_LIST_DIR}/k1_flac_p.a)
endif()
endif()


