/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief partition interface
 */

#ifndef __PARTITION_H__
#define __PARTITION_H__

#define MAX_PARTITION_COUNT	15

struct partition_entry {
	u8_t name[8];
	u8_t type;
	u8_t file_id;
	u8_t mirror_id:4;
	u8_t storage_id:4;
	u8_t flag;
	u32_t offset;
	u32_t size;
	u32_t file_offset;
}  __attribute__((packed));

#define PARTITION_FLAG_ENABLE_CRC			(1 << 0)
#define PARTITION_FLAG_ENABLE_ENCRYPTION	(1 << 1)
#define PARTITION_FLAG_BOOT_CHECK			(1 << 2)

#define PARTITION_MIRROR_ID_A				(0)
#define PARTITION_MIRROR_ID_B				(1)
#define PARTITION_MIRROR_ID_INVALID			(0xf)

#define PARTITION_TYPE_INVALID				(0)
#define PARTITION_TYPE_BOOT					(1)
#define PARTITION_TYPE_SYSTEM				(2)
#define PARTITION_TYPE_RECOVERY				(3)
#define PARTITION_TYPE_DATA					(4)
#define PARTITION_TYPE_TEMP					(5)
#define PARTITION_TYPE_PARAM				(6)

/* predefined file ids */
#define PARTITION_FILE_ID_INVALID			(0)
#define PARTITION_FILE_ID_BOOT				(1)
#define PARTITION_FILE_ID_PARAM				(2)
#define PARTITION_FILE_ID_RECOVERY			(3)
#define PARTITION_FILE_ID_SYSTEM			(4)
#define PARTITION_FILE_ID_SDFS				(5)
#define PARTITION_FILE_ID_NVRAM_FACTORY		(6)
#define PARTITION_FILE_ID_NVRAM_FACTORY_RW	(7)
#define PARTITION_FILE_ID_NVRAM_USER		(8)
#define PARTITION_FILE_ID_FAT_FLASH			(9)
#define PARTITION_FILE_ID_FAT_NOR			(10)

#define PARTITION_FILE_ID_COREDUMP          (0xf0)
#define PARTITION_FILE_ID_RECORDER_RAW		(0xfc)
#define PARTITION_FILE_ID_EVTBUF            (0xfd)
#define PARTITION_FILE_ID_OTA_TEMP			(0xfe)
#define PARTITION_FILE_ID_RESERVED			(0xff)

const struct partition_entry *partition_get_part(u8_t file_id);
const struct partition_entry *partition_get_mirror_part(u8_t file_id);
const struct partition_entry *partition_find_part(u32_t nor_phy_addr);
const struct partition_entry *partition_get_part_by_id(u8_t part_id);
const struct partition_entry *partition_get_temp_part(void);

u8_t partition_get_current_file_id(void);
u8_t partition_get_current_mirror_id(void);
int partition_is_mirror_part(const struct partition_entry *part);

int partition_get_max_file_size(const struct partition_entry *part);
int partition_file_mapping(u8_t file_id, u32_t vaddr);
int partition_is_param_part(const struct partition_entry *part);
int partition_is_boot_part(const struct partition_entry *part);
const struct partition_entry *partition_get_boot_mirror_part(u8_t file_id);
u32_t partition_get_partition_nor_addr(uint32_t vaddr);

#endif /* __PARTITION_H__ */
