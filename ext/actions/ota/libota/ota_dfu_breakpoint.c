/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA upgrade interface
 */

#define SYS_LOG_LEVEL 3
#define SYS_LOG_DOMAIN "otalib"
#include <logging/sys_log.h>

#include <kernel.h>
#include <string.h>
#include <device.h>
#include <flash.h>
#include <soc.h>
#include <fw_version.h>
#include <partition.h>
#include <mem_manager.h>
#include <crc.h>
#include "ota_manifest.h"
#include "ota_breakpoint.h"
#include <os_common_api.h>
#include <logging/sys_log.h>
#include <acts_ringbuf.h>
#include <partition.h>
#include <nvram_config.h>

#define OTA_DFU_BREAKPOINT_KEY_NAME "OTA_DFU_BP"

struct ota_breakpoint_dfu_info{
	u32_t header_file_crc_value;
	u32_t dfu_breakpoint;
	u32_t dfu_crc;
	u32_t dfu_size;
	u32_t dfu_version;
	u32_t dfu_packet_lenth;
	u32_t dfu_header_len;
} __attribute__((packed));


struct ota_upgrade_check_info{
	struct ota_breakpoint bp;
	struct ota_breakpoint_dfu_info dfu_bp;
};

void ota_dfu_breakpoint_dump(struct ota_breakpoint_dfu_info *bp)
{
	printk("dfubpdump: bpcrc=%x bp=%x headcrc=%x packetlen=%d version=%d\n", bp->dfu_crc, \
		bp->dfu_breakpoint, bp->header_file_crc_value, bp->dfu_packet_lenth, bp->dfu_version);
}


int ota_dfu_breakpoint_save(struct ota_breakpoint_dfu_info *bp)
{
	int err;

	ota_dfu_breakpoint_dump(bp);

	err = nvram_config_set(OTA_DFU_BREAKPOINT_KEY_NAME, bp, sizeof(struct ota_breakpoint_dfu_info));
	if (err) {
		return -1;
	}

	return 0;
}

int ota_dfu_breakpoint_load(struct ota_breakpoint_dfu_info *bp)
{
	int rlen;

	rlen = nvram_config_get(OTA_DFU_BREAKPOINT_KEY_NAME, bp, sizeof(struct ota_breakpoint_dfu_info));
	if (rlen != sizeof(struct ota_breakpoint_dfu_info)) {
		memset(bp, 0, sizeof(struct ota_breakpoint_dfu_info));
		SYS_LOG_INF("cannot found %s", OTA_DFU_BREAKPOINT_KEY_NAME);
		return -1;
	}

	ota_dfu_breakpoint_dump(bp);

	return 0;
}

static int ota_dfu_breakpoint_reset(struct ota_breakpoint_dfu_info *bp)
{
	memset(bp, 0, sizeof(struct ota_breakpoint_dfu_info));
	bp->dfu_version = fw_version_get_full_version();
	return ota_dfu_breakpoint_save(bp);
}

int ota_dfu_breakpoint_clear(void)
{
	SYS_LOG_INF();
	return nvram_config_set(OTA_DFU_BREAKPOINT_KEY_NAME, NULL, 0);
}

int ota_dfu_breakpoint_set_dfu_info(u32_t breakpoint, u32_t crc, u32_t size, u32_t version)
{
	int err;
	struct ota_breakpoint_dfu_info bp;
	struct ota_breakpoint ota_bp;

	err = ota_dfu_breakpoint_load(&bp);
	if (err) {
		SYS_LOG_INF("no bp in nvram, use default bp");
		ota_dfu_breakpoint_reset(&bp);
	}

	bp.dfu_breakpoint = breakpoint;
	bp.dfu_crc = crc;
	bp.dfu_size = size;
	bp.dfu_version = version;

	if(crc == 0 && size == 0){
		bp.header_file_crc_value = 0;
		ota_breakpoint_reset(&ota_bp);
	}

	ota_dfu_breakpoint_save(&bp);

	return 0;
}

int ota_dfu_breakpoint_get_dfu_info(u32_t *breakpoint, u32_t *crc, u32_t *size, u32_t *version)
{
	int err;
	struct ota_breakpoint_dfu_info bp;

	err = ota_dfu_breakpoint_load(&bp);
	if (err) {
		SYS_LOG_INF("no bp in nvram, use default bp");
		ota_dfu_breakpoint_reset(&bp);
	}

	*breakpoint = bp.dfu_breakpoint;
	*crc = bp.dfu_crc;
	*size = bp.dfu_size;
	*version = bp.dfu_version;

	return 0;
}

int ota_dfu_breakpoint_get_packet_length(void)
{
	int err;
	struct ota_breakpoint_dfu_info bp;

	err = ota_dfu_breakpoint_load(&bp);
	if (err) {
		SYS_LOG_INF("no bp in nvram, use default bp");
		ota_dfu_breakpoint_reset(&bp);
	}

	return bp.dfu_packet_lenth;
}

int ota_dfu_breakpoint_get_header_crc(void)
{
	int err;
	struct ota_breakpoint_dfu_info bp;

	err = ota_dfu_breakpoint_load(&bp);
	if (err) {
		SYS_LOG_INF("no bp in nvram, use default bp");
		ota_dfu_breakpoint_reset(&bp);
	}

	return bp.header_file_crc_value;
}


int ota_dfu_breakpoint_set_breakpoint(u32_t breakpoint)
{
	int err;
	struct ota_breakpoint_dfu_info bp;

	err = ota_dfu_breakpoint_load(&bp);
	if (err) {
		SYS_LOG_INF("no bp in nvram, use default bp");
		ota_dfu_breakpoint_reset(&bp);
	}

	bp.dfu_breakpoint = breakpoint;

	ota_dfu_breakpoint_save(&bp);

	return 0;
}

int ota_dfu_breakpoint_save_header_crc(uint32_t header_crc, uint32_t packet_length, uint32_t header_len)
{
	int err;
	struct ota_breakpoint_dfu_info bp;

	err = ota_dfu_breakpoint_load(&bp);
	if (err) {
		SYS_LOG_INF("no bp in nvram, use default bp");
		ota_dfu_breakpoint_reset(&bp);
	}

	bp.header_file_crc_value = header_crc;

	bp.dfu_packet_lenth = packet_length;

	bp.dfu_header_len = header_len;

	ota_dfu_breakpoint_save(&bp);

	return 0;
}

int ota_duf_breakpoint_load_header_data(const char *dev_name, uint8_t *data_buffer, int data_len)
{
	struct device *temp_part_dev;
	const struct partition_entry *temp_part;

	temp_part_dev = device_get_binding(dev_name);
	if (!temp_part_dev) {
		SYS_LOG_ERR("cannot found temp part device %s", dev_name);
		return -EINVAL;
	}

	temp_part = partition_get_temp_part();
	if (temp_part == NULL) {
		SYS_LOG_ERR("cannot found temp partition to store ota fw");
		return -EINVAL;
	}

	printk("read part offset %x\n", temp_part->offset);

	flash_read(temp_part_dev, temp_part->offset, data_buffer, data_len);

	if(data_buffer[0] != 0x41 && data_buffer[1] != 0x4F \
		&& data_buffer[2] != 0x54 && data_buffer[3] != 0x41){
		data_buffer[0] = 0x41;
		data_buffer[1] = 0x4F;
		data_buffer[2] = 0x54;
		data_buffer[3] = 0x41;
	}

	print_buffer((const void *)data_buffer, 1, 128, 16, -1);

	return 0;

}

int ota_duf_breakpoint_load_header_data_crc(const char *dev_name, uint8_t *data_buffer, int data_buffer_len, int data_len)
{
	struct device *temp_part_dev;
	const struct partition_entry *temp_part;
	int read_once_len;
	int offset;
	int header_crc;

	temp_part_dev = device_get_binding(dev_name);
	if (!temp_part_dev) {
		SYS_LOG_ERR("cannot found temp part device %s", dev_name);
		return 0;
	}

	temp_part = partition_get_temp_part();
	if (temp_part == NULL) {
		SYS_LOG_ERR("cannot found temp partition to store ota fw");
		return 0;
	}

	printk("read part offset %x\n", temp_part->offset);

	offset = 0;
	header_crc = 0;
	while(data_len){
		if(data_len > data_buffer_len){
			read_once_len = data_buffer_len;
		}else{
			read_once_len = data_len;
		}

		flash_read(temp_part_dev, temp_part->offset + offset, data_buffer, read_once_len);

		if(data_buffer[0] != 0x41 && data_buffer[1] != 0x4F \
			&& data_buffer[2] != 0x54 && data_buffer[3] != 0x41 && offset == 0){

			data_buffer[0] = 0x41;
			data_buffer[1] = 0x4F;
			data_buffer[2] = 0x54;
			data_buffer[3] = 0x41;
		}

		header_crc = utils_crc32(header_crc, data_buffer, read_once_len);

		data_len -= read_once_len;

		offset += read_once_len;
	}

	return header_crc;
}


int ota_dfu_breakpoint_check(const char *dev_name, u32_t max_packet_length, int *bp_offset)
{
	int err;
	u32_t header_crc;
	struct ota_upgrade_check_info *info;
	char image_buf[128];

	info = mem_malloc(sizeof(struct ota_upgrade_check_info));
	if(!info){
		*bp_offset = 0;
		return -ENOMEM;
	}

	err = ota_breakpoint_load(&info->bp);
	if(err){
		*bp_offset = 0;
		mem_free(info);
		return -EIO;
	}

	err = ota_dfu_breakpoint_load(&info->dfu_bp);
	if(err){
		ota_breakpoint_reset(&info->bp);
		*bp_offset = 0;
		mem_free(info);
		return -EIO;
	}

	if(info->dfu_bp.header_file_crc_value == 0){
		goto err_deal;
	}

	if(info->dfu_bp.dfu_packet_lenth == 0 || info->dfu_bp.dfu_packet_lenth > max_packet_length){
		goto err_deal;
	}

	if(info->dfu_bp.dfu_header_len != CONFIG_OTA_UPGRADE_SAVE_OTA_HEADER_LEN){
		goto err_deal;
	}

	header_crc = ota_duf_breakpoint_load_header_data_crc(dev_name, image_buf, sizeof(image_buf), CONFIG_OTA_UPGRADE_SAVE_OTA_HEADER_LEN);

	if(header_crc != info->dfu_bp.header_file_crc_value){
		SYS_LOG_ERR("header crc error %x %x", header_crc, info->dfu_bp.header_file_crc_value);
		goto err_deal;
	}

	if(info->dfu_bp.dfu_breakpoint != info->bp.cur_file_write_offset){
		SYS_LOG_WRN("breakpoint err %x to %x\n", info->dfu_bp.dfu_breakpoint, info->bp.cur_file_write_offset);
		goto err_deal;
	}else{
		SYS_LOG_INF("get breakpoint %x\n", info->bp.cur_file_write_offset);
	}

	*bp_offset = info->bp.cur_file_write_offset;
	mem_free(info);

	return 0;

err_deal:
	ota_breakpoint_reset(&info->bp);
	ota_dfu_breakpoint_reset(&info->dfu_bp);
	*bp_offset = 0;
	mem_free(info);
	return -EINVAL;
}

