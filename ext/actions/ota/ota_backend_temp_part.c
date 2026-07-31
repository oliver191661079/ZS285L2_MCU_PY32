/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA SDCARD backend interface
 */

#define SYS_LOG_LEVEL 3
#define SYS_LOG_DOMAIN "otapart"
#include <logging/sys_log.h>

#include <kernel.h>
#include <string.h>
#include <flash.h>
#include <partition.h>
#include <mem_manager.h>
#include <ota_backend.h>
#include <ota_backend_temp_part.h>
#include <os_common_api.h>
#include <image.h>
#include <minlzma.h>
#include <watchdog_hal.h>
#include <soc.h>

enum ota_backend_temp_type {
	OTA_BACKEND_SPINOR,
	OTA_BACKEND_SD,
	OTA_BACKEND_SPINOR_EXT,
};

#define OTA_BACKEND_SD_SECTOR_SIZE (512)
#ifndef CONFIG_MMC_SDCARD_DEV_NAME
#define CONFIG_MMC_SDCARD_DEV_NAME "sd"
#endif

struct ota_backend_temp_part {
	struct ota_backend backend;
	struct device *dev;
	const char *dev_name;
	uint32_t offset;
	int size;
	int type; // refer to enum ota_backend_temp_type
	int lzma; // lzma compress flag
	int original_size;
};

#define LZMA_MAGIC      0x414d5a4c

typedef struct lzma_head {
	uint32_t  ih_magic;
	uint32_t ih_hdr_size;    /* Size of image header (bytes). */
	uint32_t ih_img_size;    /* Size of image body (bytes). */
	uint32_t ih_org_size;   /* Size of origin data (bytes) */
}lzma_head_t;

typedef struct lzma_mcu_head {
	uint32_t original_size;
	uint32_t  ih_magic;
	uint32_t ih_hdr_size;    /* Size of image header (bytes). */
	uint32_t ih_img_size;    /* Size of image body (bytes). */
	uint32_t ih_org_size;   /* Size of origin data (bytes) */
}lzma_mcu_head_t;


#define LZMA_IN_MAX			(32*1024)
#define LZMA_OUT_MAX		(32*1024)

static uint8_t __ota_bss lzma_inbuf[LZMA_IN_MAX];
static uint8_t __ota_bss lzma_outbuf[LZMA_OUT_MAX];
static uint32_t lzma_inoff = 0;
static uint32_t lzma_outoff = 0xffffffff;

static int temp_part_read_flash(struct ota_backend_temp_part *ob_tp, int offset, uint8_t *buf, int size)
{
	int err = 0;
	uint32_t offs = ob_tp->offset + offset;
	uint32_t head, count, left;
	uint8_t *buffer = NULL;
	uint8_t *src_addr = (uint8_t *)CONFIG_FLASH_BASE_ADDRESS;

	SYS_LOG_DBG("dev %p %d: offset 0x%x, size 0x%x, buf %p",
		ob_tp->dev, ob_tp->type, offset, size, buf);

	if ((offset + size) > ob_tp->size) {
		SYS_LOG_ERR("offs 0x%x size 0x%x is too big, max size 0x%x",
			offset, size, ob_tp->size);

		return -EINVAL;
	}

	if (OTA_BACKEND_SD == ob_tp->type) {
		head = ROUND_UP(offs, OTA_BACKEND_SD_SECTOR_SIZE) - offs;
		size = (size > head) ? (size - head): 0;
		count = size / OTA_BACKEND_SD_SECTOR_SIZE;
		left = size % OTA_BACKEND_SD_SECTOR_SIZE;
		offs /= OTA_BACKEND_SD_SECTOR_SIZE;

		if (head || left) {
			buffer = (uint8_t *)mem_malloc(OTA_BACKEND_SD_SECTOR_SIZE);
			if (!buffer) {
				SYS_LOG_ERR("can not malloc %d size", OTA_BACKEND_SD_SECTOR_SIZE);
				return -ENOMEM;
			}
		}

		if (head && !err) {
			err = flash_read(ob_tp->dev, offs, buffer, 1);
			if (err) {
				SYS_LOG_ERR("read error %d, offs 0x%x, count %d",
					err, offs, count);
				err = -EIO;
			}
			memcpy(buf, buffer + OTA_BACKEND_SD_SECTOR_SIZE - head, head);
			buf += head;
			offs += 1;
		}

		if (count && !err) {
			err = flash_read(ob_tp->dev, offs, buf, count);
			if (err) {
				SYS_LOG_ERR("read error %d, offs 0x%x, count %d",
					err, offs, count);
				err = -EIO;
			}
			buf += (count * OTA_BACKEND_SD_SECTOR_SIZE);
			offs += count;
		}

		if (left && !err) {
			err = flash_read(ob_tp->dev, offs, buffer, 1);
			if (err) {
				SYS_LOG_ERR("read error %d, offs 0x%x, count %d",
					err, offs, count);
				err = -EIO;
			}
			memcpy(buf, buffer, left);
		}

		if (buffer) {
			mem_free(buffer);
		}
	}else if(OTA_BACKEND_SPINOR_EXT == ob_tp->type) {
		err = flash_read(ob_tp->dev, offs, buf, size);
		if (err) {
			err = -EIO;
		}
	}else {
		memcpy(buf, src_addr + offs, size);
		err = 0;
	}

	return err;
}

static int temp_part_read_lzma(struct ota_backend_temp_part *ob_tp, int offset, uint8_t *buf, int size)
{
	int err, ret;
	uint32_t out_off, out_size;
	lzma_head_t *plzma_h = (lzma_head_t *)lzma_inbuf;
	
	// read first part from lzma buffer
	if ((offset >= lzma_outoff) && (offset < (lzma_outoff + LZMA_OUT_MAX))) {
		if ((offset + size) > (lzma_outoff + LZMA_OUT_MAX)) {
			out_size = lzma_outoff + LZMA_OUT_MAX - offset;
			memcpy(buf, lzma_outbuf+offset-lzma_outoff, out_size);
			buf += out_size;
			offset += out_size;
			size -= out_size;
		}
	}

	// check size
	if (size <= 0) {
		return 0;
	}


	// check offset
	if ((offset < lzma_outoff) || (offset >= (lzma_outoff + LZMA_OUT_MAX))) {
		// init lzma block
		if (offset < lzma_outoff) {
			lzma_inoff = 0;
			out_off = 0;
		} else {
			out_off = lzma_outoff;
		}

		// search lzma block
		while (1) {
			// read lzma header
			err = temp_part_read_flash(ob_tp, lzma_inoff, (uint8_t*)plzma_h, sizeof(lzma_head_t));
			if (err) {
				return -1;
			}

			// check lzma magic
			if (plzma_h->ih_magic != LZMA_MAGIC) {
				SYS_LOG_ERR("lzma error magic: 0x%x", plzma_h->ih_magic);
				return -1;
			}

			// check out offset
			if ((offset >= out_off) && (offset < (out_off + LZMA_OUT_MAX))) {
				break;
			}

			// next lzma block
			lzma_inoff += plzma_h->ih_hdr_size + plzma_h->ih_img_size;
			out_off += plzma_h->ih_org_size;


			//if(OTA_BACKEND_SPINOR_EXT == ob_tp->type && lzma_inoff >= ob_tp->original_size){
			//	break;
			//}
		}

		// check lzma buffer size
		if (plzma_h->ih_org_size > LZMA_OUT_MAX) {
			SYS_LOG_ERR("lzma buffer too small! size 0x%x", LZMA_OUT_MAX);
			return -1;
		}

		// read lzma img
		err = temp_part_read_flash(ob_tp, lzma_inoff, lzma_inbuf, plzma_h->ih_hdr_size+plzma_h->ih_img_size);
		if (err) {
			return -1;
		}

#ifdef CONFIG_WATCHDOG
		watchdog_clear();
#endif
		// lzma decompress
		out_size = LZMA_OUT_MAX;
		ret = XzDecode(lzma_inbuf+plzma_h->ih_hdr_size, plzma_h->ih_img_size, lzma_outbuf, &out_size);
		if (ret == 0) {
			SYS_LOG_ERR("XzDecode error! size 0x%x", out_size);
			return -2;
		}

		// check origin size
		if (out_size != plzma_h->ih_org_size) {
			SYS_LOG_ERR("XzDecode out_size mismatch! 0x%x", out_size);
			return -3;
		}

		lzma_outoff = out_off;
		SYS_LOG_DBG("XzDecode 0x%x ok! 0x%x -> 0x%x", out_off, plzma_h->ih_img_size, plzma_h->ih_org_size);
	}

	// read last part from lzma buffer
	memcpy(buf, lzma_outbuf+offset-lzma_outoff, size);

	return 0;
}

static int ota_backend_temp_part_prepare(struct ota_backend_temp_part *ob_tp)
{
	int err;
	uint32_t inner_offset = 0;
	image_head_t *pimg_h = (image_head_t *)lzma_inbuf;
	lzma_head_t *plzma_h = (lzma_head_t *)lzma_inbuf;

	// init lzma flag
	ob_tp->lzma = 0;
	
#ifdef CONFIG_OTA_TEMP_BIN_NOT_ENCRYPT
	memcpy(plzma_h, (uint8_t *)CONFIG_FLASH_BASE_ADDRESS, sizeof(image_head_t));
#else
	// read image header
	err = temp_part_read_flash(ob_tp, 0x00, (uint8_t*)pimg_h, sizeof(image_head_t));
	if (err) {
		return -1;
	}
#endif	
	// check ota_app
	if ((pimg_h->magic0 == IMAGE_MAGIC0) && (pimg_h->magic1 == IMAGE_MAGIC1)) {
		inner_offset = pimg_h->header_size + pimg_h->body_size;
		inner_offset = (inner_offset + OTA_BACKEND_SD_SECTOR_SIZE - 1) & ~(OTA_BACKEND_SD_SECTOR_SIZE - 1);
		SYS_LOG_INF("found app! temp inner offset: 0x%x", inner_offset);
	}

	// update temp offset and size
	if (inner_offset > 0) {
		ob_tp->offset += inner_offset;
		ob_tp->size -= inner_offset;
	}

	// read lzma header
	err = temp_part_read_flash(ob_tp, 0, (uint8_t*)plzma_h, sizeof(lzma_head_t));
	if (err) {
		return -1;
	}

	// check lzma header
	if (plzma_h->ih_magic == LZMA_MAGIC) {
		// set lzma flag
		ob_tp->lzma = 1;
		SYS_LOG_INF("found lzma 0x%x\n !", ob_tp->offset);
	}

	return 0;
}

int ota_backend_temp_part_read(struct ota_backend *backend, int offset, void *buf, int size)
{
	struct ota_backend_temp_part *ob_tp = CONTAINER_OF(backend,
		struct ota_backend_temp_part, backend);
	int err = 0;
	
	// check lzma flag
	if (ob_tp->lzma) {
		err = temp_part_read_lzma(ob_tp, offset, (uint8_t*)buf, size);
	} else {
		err = temp_part_read_flash(ob_tp, offset, (uint8_t*)buf, size);
	}

	return err;
}

int ota_backend_temp_part_open(struct ota_backend *backend)
{
#if 0
	struct ota_backend_temp_part *backend_temp_part = CONTAINER_OF(backend,
		struct ota_backend_temp_part, backend);

	SYS_LOG_DBG("dev %s: open: type %d", backend_temp_part->dev_name, backend->type);
#endif
	return 0;
}

int ota_backend_temp_part_close(struct ota_backend *backend)
{
#if 0
	struct ota_backend_temp_part *backend_temp_part = CONTAINER_OF(backend,
		struct ota_backend_temp_part, backend);

	SYS_LOG_DBG("dev %s: close: type %d", backend_temp_part->dev_name, backend->type);
#endif

	backend->cb(backend, OTA_BACKEND_UPGRADE_STATE, 0);

	return 0;
}

void ota_backend_temp_part_exit(struct ota_backend *backend)
{
	struct ota_backend_temp_part *backend_temp_part = CONTAINER_OF(backend,
		struct ota_backend_temp_part, backend);

	SYS_LOG_INF("dev %s: exit: type %d", backend_temp_part->dev_name, backend->type);

	mem_free(backend_temp_part);
}


const struct ota_backend_api ota_backend_api_temp_part = {
	.init = (void *)ota_backend_temp_part_init,
	.exit = ota_backend_temp_part_exit,
	.open = ota_backend_temp_part_open,
	.close = ota_backend_temp_part_close,
	.read = ota_backend_temp_part_read,
};

#ifdef CONFIG_OTA_TEMP_BIN_NOT_ENCRYPT
int ota_backend_calc_external_temp_bin_offset(const struct partition_entry *temp_part)
{
	const struct partition_entry *recovery_part;
	uint32_t nor_mapping_addr;

	nor_mapping_addr = soc_memctrl_cpu_to_nor_phy_addr(CONFIG_FLASH_BASE_ADDRESS);

	recovery_part = partition_get_part(PARTITION_FILE_ID_RECOVERY);
	if (recovery_part == NULL) {
		SYS_LOG_ERR("cannot found recovery partition to store ota fw");
		return 0;
	}	

	SYS_LOG_INF("recovery part addr 0x%x mapping addr 0x%x\n", recovery_part->offset, nor_mapping_addr);

	return temp_part->offset + nor_mapping_addr - recovery_part->offset;
}
#endif

struct ota_backend *ota_backend_temp_part_init(ota_backend_notify_cb_t cb,
		struct ota_backend_temp_part_init_param *param)
{
	struct ota_backend_temp_part *backend_temp_part;
	const struct partition_entry *temp_part;
	struct device *temp_part_dev;

	SYS_LOG_DBG("init backend %s\n", param->dev_name);


	temp_part_dev = device_get_binding(param->dev_name);
	if (!temp_part_dev) {
		SYS_LOG_ERR("cannot found temp part device %s", param->dev_name);
		return NULL;
	}

	temp_part = partition_get_temp_part();
	if (temp_part == NULL) {
		SYS_LOG_ERR("cannot found temp partition to store ota fw");
		return NULL;
	}


	SYS_LOG_INF("temp partition offset 0x%x, size 0x%x\n",
		temp_part->offset, temp_part->size);

	backend_temp_part = mem_malloc(sizeof(struct ota_backend_temp_part));
	if (!backend_temp_part) {
		SYS_LOG_ERR("malloc failed");
		return NULL;
	}

	memset(backend_temp_part, 0x0, sizeof(struct ota_backend_temp_part));
	backend_temp_part->dev = temp_part_dev;
	backend_temp_part->dev_name = param->dev_name;
#ifdef CONFIG_OTA_TEMP_BIN_NOT_ENCRYPT
	backend_temp_part->offset = ota_backend_calc_external_temp_bin_offset(temp_part);
#else
	backend_temp_part->offset = 0;
#endif
	backend_temp_part->size = temp_part->size;

	lzma_inoff = 0;
	lzma_outoff = 0xffffffff;

	if (!strcmp(param->dev_name, CONFIG_MMC_SDCARD_DEV_NAME)) {
		backend_temp_part->type = OTA_BACKEND_SD;
		if (flash_init(backend_temp_part->dev)) {
			SYS_LOG_ERR("init sd device error");
			return NULL;
		}
	}else if (strcmp(param->dev_name, CONFIG_XSPI1_NOR_ACTS_DEV_NAME) == 0){
		backend_temp_part->type = OTA_BACKEND_SPINOR_EXT;
	}else {
		backend_temp_part->type = OTA_BACKEND_SPINOR;
	}

	ota_backend_temp_part_prepare(backend_temp_part);

	ota_backend_init(&backend_temp_part->backend, OTA_BACKEND_TYPE_TEMP_PART,
			 (struct ota_backend_api *)&ota_backend_api_temp_part, cb);

	/* spinor backend detected, notify app */
	cb(&backend_temp_part->backend, OTA_BACKEND_UPGRADE_STATE, 1);

	return &backend_temp_part->backend;
}

void *ota_backend_temp_part_file_open(const char *dev_name, uint32_t file_length, uint32_t file_offset)
{
	int err;
	struct device *temp_part_dev;
	struct ota_backend_temp_part *backend_temp_part;
	lzma_mcu_head_t *plzma_h = (lzma_mcu_head_t *)lzma_inbuf;

	temp_part_dev = device_get_binding(dev_name);
	if (!temp_part_dev) {
		SYS_LOG_ERR("cannot found temp part device %s", dev_name);
		return NULL;
	}

	//printk("get temp part offset %x size %x\n", temp_part->offset, temp_part->size);

	backend_temp_part = mem_malloc(sizeof(struct ota_backend_temp_part));
	if (!backend_temp_part) {
		SYS_LOG_ERR("malloc failed");
		return NULL;
	}

	memset(backend_temp_part, 0x0, sizeof(struct ota_backend_temp_part));
	backend_temp_part->dev = temp_part_dev;
	backend_temp_part->dev_name = dev_name;
	backend_temp_part->offset = file_offset;
	backend_temp_part->size = file_length;

	lzma_inoff = 0;
	lzma_outoff = 0xffffffff;

	if (!strcmp(dev_name, CONFIG_MMC_SDCARD_DEV_NAME)) {
		backend_temp_part->type = OTA_BACKEND_SD;
		if (flash_init(backend_temp_part->dev)) {
			SYS_LOG_ERR("init sd device error");
			return NULL;
		}
	}else if (strcmp(dev_name, CONFIG_XSPI1_NOR_ACTS_DEV_NAME) == 0){
		backend_temp_part->type = OTA_BACKEND_SPINOR_EXT;
	}else {
		backend_temp_part->type = OTA_BACKEND_SPINOR;
	}

	// read lzma header
	err = temp_part_read_flash(backend_temp_part, 0, (uint8_t*)plzma_h, sizeof(lzma_mcu_head_t));

	//print_buffer((const void *)plzma_h, 1, sizeof(lzma_head_t), 16, -1);

	if (err) {
		mem_free(backend_temp_part);
		return NULL;
	}

	// check lzma header
	if (plzma_h->ih_magic == LZMA_MAGIC) {
		// set lzma flag
		backend_temp_part->lzma = 1;
		SYS_LOG_INF("found lzma!");
	}

	backend_temp_part->original_size = plzma_h->original_size;

	backend_temp_part->offset += 4;

	return (void *)backend_temp_part;

}

int ota_backend_temp_part_file_get_length(void *handle)
{
	struct ota_backend_temp_part *backend_temp_part = (struct ota_backend_temp_part *)handle;

	return backend_temp_part->size;
}

int ota_backend_temp_part_file_get_original_length(void *handle)
{
	struct ota_backend_temp_part *backend_temp_part = (struct ota_backend_temp_part *)handle;

	return backend_temp_part->original_size;
}


int ota_backend_temp_part_file_read(void *handle, uint8_t *buf, uint32_t offset, uint32_t size)
{
	int err;
	struct ota_backend_temp_part *backend_temp_part = (struct ota_backend_temp_part *)handle;

	// check lzma flag
	if (backend_temp_part->lzma) {
		err = temp_part_read_lzma(backend_temp_part, offset, (uint8_t*)buf, size);
	} else {
		err = temp_part_read_flash(backend_temp_part, offset, (uint8_t*)buf, size);
	}

	return err;
}

int ota_backend_temp_part_file_close(void *handle)
{
	mem_free(handle);

	return 0;
}

