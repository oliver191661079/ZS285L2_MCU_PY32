/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA breakpoint interface
 */

#ifndef __OTA_DUF_BREAKPOINT_H__
#define __OTA_DFU_BREAKPOINT_H__

int ota_dfu_breakpoint_clear(void);

int ota_dfu_breakpoint_set_dfu_info(u32_t breakpoint, u32_t crc, u32_t size, u32_t version);

int ota_dfu_breakpoint_get_dfu_info(u32_t *breakpoint, u32_t *crc, u32_t *size, u32_t *version);

int ota_dfu_breakpoint_get_packet_length(void);

int ota_dfu_breakpoint_get_header_crc(void);

int ota_dfu_breakpoint_set_breakpoint(u32_t breakpoint);

int ota_dfu_breakpoint_save_header_crc(uint32_t header_crc, uint32_t packet_length, uint32_t header_len);

int ota_duf_breakpoint_load_header_data(const char *dev_name, uint8_t *data_buffer, int data_len);

int ota_dfu_breakpoint_check(const char *dev_name, u32_t max_packet_length, int *bp_offset);

#endif
