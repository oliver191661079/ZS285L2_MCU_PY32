/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA storage interface
 */

#define SYS_LOG_LEVEL 3
#define SYS_LOG_DOMAIN "otalib"
#include <logging/sys_log.h>

#include <kernel.h>
#include <flash.h>
#include <string.h>
#include <mem_manager.h>
#include "ota_storage.h"
#include <os_common_api.h>
#ifdef CONFIG_WATCHDOG
#include <watchdog_hal.h>
#endif

#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif

#include <crc.h>

#include <crypto/rsa.h>

#define XIP_DEV_NAME	CONFIG_XSPI_NOR_ACTS_DEV_NAME

enum ota_storage_type {
	OTA_STORAGE_SPINOR = 0,
#ifdef CONFIG_OTA_DOWNLOAD_EXTERNAL_FLASH
	OTA_STORAGE_EXT_NOR,
#endif
	OTA_STORAGE_MAX_TYPE
};

struct ota_storage
{
	struct device *dev;
	const char *dev_name;
	int max_write_seg;
	int storage_id;		/* code run on this device? */
	uint16_t erase_read_time;
	uint16_t erase_4kb_time;
	uint16_t erase_64kb_time;
	uint16_t write_time;
	uint16_t read_time;
	uint16_t crc_calc_time;
	uint16_t hash_calc_time;
	uint16_t erase_4kb_cnt;
	uint16_t erase_64kb_cnt;
	uint16_t skip_4kb_cnt;
	uint16_t skip_64kb_cnt;
	void *hash_ctx;
	uint8_t *sig_data;
};

static __ota_bss uint8_t ota_erase_check_buf[0x1000];

static struct ota_storage global_ota_storage[OTA_STORAGE_MAX_TYPE];

void ota_storage_set_max_write_seg(struct ota_storage *storage, int max_write_seg)
{
	if (max_write_seg <= 0)
		return;

	storage->max_write_seg = max_write_seg;
}

int ota_storage_get_storage_id(struct ota_storage *storage)
{
	return storage->storage_id;
}

int ota_storage_write(struct ota_storage *storage, int offs,
		      uint8_t *buf, int size)
{
	int wlen, err;

	// SYS_LOG_INF("offs 0x%x, buf %p, size %d", offs, buf, size);
	uint32_t start_time = k_uptime_get_32();

	wlen = storage->max_write_seg;

	while (size > 0) {
		if (size < storage->max_write_seg)
			wlen = size;

#ifdef CONFIG_WATCHDOG
		watchdog_clear();
#endif

#ifdef CONFIG_TASK_WDT
		task_wdt_feed_all();
#endif

		err = flash_write(storage->dev, offs, buf, wlen);
		if (err < 0) {
			SYS_LOG_ERR("write error %d, offs 0x%x, buf %p, size %d", err, offs, buf, size);
			return -EIO;
		}

		offs += wlen;
		buf += wlen;
		size -= wlen;
	}

	storage->write_time += (k_uptime_get_32() - start_time);

	return 0;
}

int ota_storage_read(struct ota_storage *storage, int offs,
		     uint8_t *buf, int size)
{
	int err;
	int rlen = OTA_STORAGE_DEFAULT_READ_SEGMENT_SIZE;

	uint32_t start_time = k_uptime_get_32();

	// SYS_LOG_INF("offs 0x%x, buf %p, size %d", offs, buf, size);

	while (size > 0) {
		if (size < OTA_STORAGE_DEFAULT_READ_SEGMENT_SIZE)
			rlen = size;

		err = flash_read(storage->dev, offs, buf, rlen);
		if (err < 0) {
			SYS_LOG_ERR("read error %d, offs 0x%x, buf %p, size %d", err, offs, buf, size);
			return -EIO;
		}

		offs += rlen;
		buf += rlen;
		size -= rlen;
	}

	storage->read_time += (k_uptime_get_32() - start_time);

	return 0;
}

int ota_storage_calc_crc(struct ota_storage *storage, uint32_t addr, uint32_t size, uint8_t *buf, int buf_size)
{
	int rlen;
	uint32_t crc;
	uint32_t start_time;

	crc = 0;

	rlen = buf_size;
	while (size > 0) {
		if (size < rlen)
			rlen = size;

		ota_storage_read(storage, addr, buf, rlen);

		start_time = k_uptime_get_32();
		crc = utils_crc32(crc, buf, rlen);
		storage->crc_calc_time += (k_uptime_get_32() - start_time);

		size -= rlen;
		addr += rlen;
	}

	return crc;
}

const unsigned char* ota_storage_get_image_hash(struct ota_storage *storage, uint32_t addr, uint32_t size, uint8_t *buf, int buf_size)
{
	int rlen;
	uint32_t start_time;
	uint32_t read_addr = addr;

	storage->hash_ctx = (void *)ota_erase_check_buf;

	rom_sha256_init(storage->hash_ctx);

	rlen = buf_size;
	while (size > 0) {
		if (size < rlen)
			rlen = size;

		ota_storage_read(storage, read_addr, buf, rlen);

		if(read_addr == addr){
			if(buf[0] != 0x41 && buf[1] != 0x4F \
				&& buf[2] != 0x54 && buf[3] != 0x41){
				buf[0] = 0x41;
				buf[1] = 0x4F;
				buf[2] = 0x54;
				buf[3] = 0x41;
			}
		}

		start_time = k_uptime_get_32();
		rom_sha256_update(storage->hash_ctx, buf, rlen);
		storage->crc_calc_time += (k_uptime_get_32() - start_time);

		size -= rlen;
		read_addr += rlen;
	}

	return rom_sha256_final(storage->hash_ctx);

}

int ota_storage_check_image_sig_data(struct ota_storage *storage, const char *public_key, uint32_t addr, uint32_t size, uint32_t sig_addr, uint32_t sig_size, uint8_t *buf, int buf_size)
{
	const unsigned char* hash_data;

	hash_data = ota_storage_get_image_hash(storage, addr, size, buf, buf_size);

	storage->sig_data = (void *)(ota_erase_check_buf + 0x200);

	ota_storage_read(storage, sig_addr, storage->sig_data, sig_size);

	return RSA_verify_hash(public_key, storage->sig_data, sig_size, hash_data, 32);
}


int ota_storage_is_clean(struct ota_storage *storage, int offs, int size,
			uint8_t *buf, int buf_size)
{
	int i, err, read_size;
	uint32_t *wptr = NULL;
	uint8_t *cptr;

	//SYS_LOG_INF("offs 0x%x, size %d", offs, size);

	if (((unsigned int)buf & 0x3) || (buf_size & 0x3)) {
		return -EINVAL;
	}

	read_size = (buf_size > OTA_STORAGE_DEFAULT_READ_SEGMENT_SIZE)?
			OTA_STORAGE_DEFAULT_READ_SEGMENT_SIZE : buf_size;

	wptr = (uint32_t *)buf;
	while (size > 0) {
		if (size < read_size)
			read_size = size;

		err = flash_read(storage->dev, offs, buf, read_size);
		if (err) {
			SYS_LOG_ERR("read error 0x%x, offs 0x%x, size %d", err, offs, read_size);
			return -EIO;
		}

		wptr = (uint32_t *)buf;
		for (i = 0; i < (read_size >> 2); i++) {
			if (*wptr++ != 0xffffffff)
				return 0;
		}

		offs += read_size;
		//buf += read_size;
		size -= read_size;
	}

	/* check unaligned data */
	cptr = (uint8_t *)wptr;
	for (i = 0; i < (read_size & 0x3); i++) {
		if (*cptr++ != 0xff)
			return 0;
	}

	return 1;
}

static void ota_erase_use_4kb(struct ota_storage *storage, int lba, int sectors, uint8_t *check_buffer)
{
    int erase_sectors = sectors;
    int i, need_erase;
    int *p_erase_check;

	uint32_t start_time;

    while (erase_sectors > 0) {
#ifdef CONFIG_WATCHDOG
		watchdog_clear();
#endif

#ifdef CONFIG_TASK_WDT
		task_wdt_feed_all();
#endif
		start_time = k_uptime_get_32();
		flash_read(storage->dev, lba << 9, check_buffer, 0x1000);
        need_erase = 0;
        p_erase_check = (int *)check_buffer;
        for (i= 0; i < (0x1000 / sizeof(int)); i += 4) {
            int test_value = 0xffffffff;
            test_value &= p_erase_check[i + 0];
            test_value &= p_erase_check[i + 1];
            test_value &= p_erase_check[i + 2];
            test_value &= p_erase_check[i + 3];
            if (test_value != 0xffffffff) {
                need_erase = 1;
                break;
            }
        }

		storage->erase_read_time += (k_uptime_get_32() - start_time);

        if (need_erase == 1) {
            //note : erase NOT support less than 4KB size, so must up align 4KB
			start_time = k_uptime_get_32();
			flash_erase(storage->dev, lba << 9, 0x1000);
			storage->erase_4kb_time += (k_uptime_get_32() - start_time);
			storage->erase_4kb_cnt++;
        }else{
			storage->skip_4kb_cnt++;
		}

        lba += 8;
        erase_sectors -= 8;
    }
}

static void ota_erase_use_64kb(struct ota_storage *storage, int lba, int sectors, uint8_t *check_buffer)
{
    int erase_sectors = 128;
    int i, j, need_erase;
    int *p_erase_check;
	uint32_t start_time;

    while (1) {
#ifdef CONFIG_WATCHDOG
        watchdog_clear();
#endif

#ifdef CONFIG_TASK_WDT
		task_wdt_feed_all();
#endif

        need_erase = 0;
        for (j = 0; j < 16; j++) {
			start_time = k_uptime_get_32();

			flash_read(storage->dev, (lba << 9) + (0x1000 * j), check_buffer, 0x1000);

            p_erase_check = (int *)check_buffer;
            for (i= 0; i < (0x1000 / sizeof(int)); i += 4) {
                int test_value = 0xffffffff;
                test_value &= p_erase_check[i + 0];
                test_value &= p_erase_check[i + 1];
                test_value &= p_erase_check[i + 2];
                test_value &= p_erase_check[i + 3];
                if (test_value != 0xffffffff) {
                    need_erase = 1;
                    break;
                }
            }
			storage->erase_read_time += (k_uptime_get_32() - start_time);

            if (need_erase == 1)
                break;
        }

        if (need_erase == 1) {
			start_time = k_uptime_get_32();
			flash_erase(storage->dev, (lba << 9), 0x10000);
			storage->erase_64kb_time += (k_uptime_get_32() - start_time);
			storage->erase_64kb_cnt++;
        }else{
			storage->skip_64kb_cnt++;
		}

        if (sectors > 128) {
            lba += erase_sectors;
            sectors -= erase_sectors;
        } else {
            break;
        }
    }
}
int ota_storage_erase(struct ota_storage *storage, int offs, int size)
{
    int lba = offs >> 9;
    size_t sectors = size >> 9;
    int erase_sectors;
	uint8_t *sector_buffer;

    if ((lba % 8) != 0) {
        SYS_LOG_ERR("lba invalid: lba = 0x%x, sectors = 0x%x\n", lba, sectors);
        return -1;
    }

	sector_buffer = ota_erase_check_buf;

    if ((lba % 128) != 0) {
        erase_sectors = 128 - (lba % 128);
        if (erase_sectors > sectors)
            erase_sectors = sectors;

        ota_erase_use_4kb(storage, lba, erase_sectors, sector_buffer);

        lba += erase_sectors;
        sectors -= erase_sectors;
    }

    erase_sectors = sectors / 128 * 128;
    if (erase_sectors > 0) {
        ota_erase_use_64kb(storage, lba, erase_sectors, sector_buffer);
        lba += erase_sectors;
        sectors -= erase_sectors;
    }

    if (sectors != 0) {
        ota_erase_use_4kb(storage, lba, sectors, sector_buffer);
    }


    return 0;
}

int ota_storage_erase_initial(struct ota_storage *storage, int offs, int size)
{
	SYS_LOG_INF("offs 0x%x, size %d", offs, size);
	return flash_erase(storage->dev, offs, size);
}

void ota_storage_time_statistics(struct ota_storage *storage)
{
	printk("storage erase read %d ms erase 4kb %d(%d:%d) ms erase 64kb %d(%d:%d) ms\n", storage->erase_read_time, \
		storage->erase_4kb_time, storage->erase_4kb_cnt, storage->skip_4kb_cnt,\
		storage->erase_64kb_time, storage->erase_64kb_cnt, storage->skip_64kb_cnt);

	printk("storage read time %d ms write time %d ms calc crc time %d ms\n", storage->read_time, \
		storage->write_time, storage->crc_calc_time);
}


struct ota_storage *ota_storage_init(const char *storage_name)
{
	struct ota_storage *storage = NULL;
	struct device *nor_dev;

	SYS_LOG_INF("init storage %s\n", storage_name);

	nor_dev = device_get_binding(storage_name);
	if (!nor_dev) {
		SYS_LOG_ERR("cannot found storage device %s", storage_name);
		return NULL;
	}

	if (strcmp(storage_name, CONFIG_XSPI_NOR_ACTS_DEV_NAME) == 0){
		storage = &global_ota_storage[OTA_STORAGE_SPINOR];

		memset(storage, 0x0, sizeof(struct ota_storage));

		storage->dev = nor_dev;
		storage->dev_name = storage_name;
		storage->max_write_seg = OTA_STORAGE_DEFAULT_WRITE_SEGMENT_SIZE;

		storage->storage_id = 0;

	}
#ifdef CONFIG_OTA_DOWNLOAD_EXTERNAL_FLASH
	else if (strcmp(storage_name, CONFIG_XSPI1_NOR_ACTS_DEV_NAME) == 0){

		storage = &global_ota_storage[OTA_STORAGE_EXT_NOR];

		memset(storage, 0x0, sizeof(struct ota_storage));

		storage->dev = nor_dev;
		storage->dev_name = storage_name;
		storage->max_write_seg = OTA_STORAGE_DEFAULT_WRITE_SEGMENT_SIZE;

		storage->storage_id = 1;
	}
#endif
	else {
		SYS_LOG_ERR("unmatch storage name %s\n", storage_name);
	}

	return storage;
}

void ota_storage_exit(struct ota_storage *storage)
{
	SYS_LOG_INF("exit");

	storage = NULL;
}
