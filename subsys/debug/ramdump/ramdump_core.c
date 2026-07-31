/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ramdump_core
 */
#include <stdio.h>
#include <string.h>
#include <zephyr/types.h>
#include <linker/linker-defs.h>
#include <errno.h>
#include <init.h>
#include <device.h>
#include <flash.h>
#include <partition.h>
#include "ramdump_core.h"
#include <mem_manager.h>
#include <partition.h>
#include <crc.h>
#ifdef CONFIG_WATCHDOG
#include <watchdog_hal.h>
#endif

#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif

extern uint32_t get_exception_esf_info(void);

uint32_t ram_get_unused(void)
{
	return 0x60000;
}

static int ramdump_compress_data(char *src, int ilen, char *dst)
{
	int out_sz;
	char *tmp_ptr = (char *)(ram_get_unused() + 0x10000);

#if defined(CONFIG_DEBUG_RAMDUMP_COMPRESS_USE_LZ4)
#include "lz4/lz4hc.h"
	// compress buffer
	out_sz = LZ4_compress_HC_extStateHC(dst + RAMD_BLK_SZ, src, dst, ilen, RAMD_BLK_SZ, 3);
#elif defined(CONFIG_DEBUG_RAMDUMP_COMPRESS_USE_FASTLZ)
#include "fastlz/fastlz.h"
	memcpy(tmp_ptr, src, ilen);
	out_sz = fastlz_compress_level(2, tmp_ptr, ilen, dst, (uint32_t *)(ram_get_unused() + 0x8000));
#else
	memcpy(dst, src, ilen);
	out_sz = ilen;
#endif


	printk("input addr 0x%p size 0x%x outsz 0x%x\n", src, ilen, out_sz);
	print_buffer(dst, 1, 16, 16, -1);

	return out_sz;
}

static uint32_t ramdump_save_block(struct device *dev, off_t offset, char* src, int ilen)
{
	char *dst;
	int out_sz, byte;
	lz4_head_t hdr;

	// get dst buffer
	dst = (char *)ram_get_unused();

	k_sched_lock();

	out_sz = ramdump_compress_data(src, ilen, dst);

	if (out_sz <= 0) {
		k_sched_unlock();
		return 0;
	}

	// fill header
	hdr.magic = MAGIC_LZ4;
	hdr.hdr_size = sizeof(lz4_head_t);
	hdr.org_sz = ilen;
	hdr.img_sz = out_sz;

	// align output
	byte = (out_sz % 4);
	if (byte > 0) {
		memset(dst + out_sz, 0, 4 - byte);
		out_sz += 4 - byte;
	}

	// write flash
	flash_write(dev, offset, &hdr, sizeof(lz4_head_t));
	flash_write(dev, offset + sizeof(lz4_head_t), dst, out_sz);

	k_sched_unlock();

	return (sizeof(lz4_head_t) + out_sz);
}

static uint32_t ramdump_save_region(struct device *dev, off_t offset, ramd_item_t *item)
{
	int in_sz, out_sz, in_off;
	ramd_region_t region;

	// fill ramd region header
	region.mem_addr = item->start;
	region.mem_sz = item->next - item->start;
	region.img_off = 0;
	region.img_sz = 0;

	// compress block and write flash
	in_off = 0;
	while (in_off < region.mem_sz) {
		in_sz = region.mem_sz - in_off;
		if (in_sz > RAMD_BLK_SZ) {
			in_sz = RAMD_BLK_SZ;
		}
		out_sz = ramdump_save_block(dev, offset + sizeof(ramd_region_t) + region.img_sz,
					(char*)(region.mem_addr + in_off), in_sz);
		if (out_sz <= 0) {
			return 0;
		}

		// next block
		in_off += in_sz;
		region.img_sz += out_sz;
	}

	// write ramd region header
	flash_write(dev, offset, &region, sizeof(region));

	printk("[region] mem_addr=0x%08x, mem_sz=%d, img_sz=%d\n",
		region.mem_addr, region.mem_sz, region.img_sz);

	return (sizeof(ramd_region_t) + region.img_sz);
}

static uint32_t ramdump_save_header(struct device *dev, off_t offset, int img_sz, int org_sz)
{
	ramd_head_t head;

	memset(&head, 0, sizeof(head));

	// fill ramd header
	head.magic = MAGIC_RAMD;
	head.version = 0x1;
	head.img_sz = img_sz;
	head.org_sz = org_sz;
	head.esf_addr = get_exception_esf_info();
	head.current_thread = (uint32_t)k_current_get();
	head.target_type = RAMDUMP_TARGET_TYPE_CK802;

	// write ramd header
	flash_write(dev, offset, &head, sizeof(head));

	printk("[ramdump] org_sz=%d, img_sz=%d\n", head.org_sz, head.img_sz);

	return (sizeof(ramd_head_t));
}

static uint32_t ramdump_get_header(struct device *dev, off_t offset, ramd_head_t *phead)
{
	// read ramd header
	flash_read(dev, offset, phead, sizeof(ramd_head_t));

	// check ramd header
	if (phead->magic != MAGIC_RAMD) {
		return 0;
	}

	return (sizeof(ramd_head_t));
}

const struct partition_entry *ramdump_get_used_partition(uint8_t partition_id)
{
	int i;
	const struct partition_entry *part;

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		part = partition_get_part_by_id(i);
		if (part == NULL)
			break;

		if (part->file_id == 0)
			continue;

#ifndef CONFIG_OTA_RECOVERY
		/* skip current firmware's partitions */
		if (!partition_is_mirror_part(part)) {
			continue;
		}
#endif
		if (part->file_id != partition_id){
			continue;
		}

		return part;
	}

	return NULL;
}

static struct device* ramdump_get_device(int *part_offset, int *part_size)
{
	const struct partition_entry *parti;
	struct device *nor_disk;

	nor_disk = device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);
	if(nor_disk == NULL){
		printk("nor dev fail\n");
		return NULL;
	}

	parti = ramdump_get_used_partition(PARTITION_FILE_ID_EVTBUF);
	if(parti == NULL){
		printk("get ota temp part fail\n");
		return NULL;
	}

	*part_offset = parti->offset;
	*part_size = parti->size;

	return nor_disk;
}


int ramdump_save(void)
{
	struct device *nor_disk;
	int part_offset, part_size;
	ramd_item_t *item;
	int img_sz, org_sz, out_sz;
	uint32_t cycle_start, cost_ms;

	nor_disk = ramdump_get_device(&part_offset, &part_size);
	if (nor_disk == NULL){
		printk("nor dev fail\n");
		return -1;
	}
	flash_erase(nor_disk, part_offset, part_size);

	// save ramd regions
	img_sz = 0;
	org_sz = 0;
	item = (ramd_item_t*)ramdump_mem_regions;

	cycle_start = k_cycle_get_32();
	while ((item->start != 0) && (item->next != 0)) {
		out_sz = ramdump_save_region(nor_disk, part_offset + sizeof(ramd_head_t) + img_sz, item);
		if (out_sz <= 0) {
			return -2;
		}

		// next region
		org_sz += (item->next - item->start);
		img_sz += out_sz;
		item ++;
	}

	// save ramd header
	ramdump_save_header(nor_disk, part_offset, img_sz, org_sz);

	cost_ms = (k_cycle_get_32() - cycle_start) / 24000;
	printk("ramdump save %d ms\n", cost_ms);

	return 0;
}

int ramdump_get(int *part_offset, int *part_size)
{
	struct device *nor_disk;
	ramd_head_t head;
	int out_sz;

	nor_disk = ramdump_get_device(part_offset, part_size);
	if (nor_disk == NULL){
		printk("nor dev fail\n");
		return -1;
	}

	// get ramdump header
	out_sz = ramdump_get_header(nor_disk, *part_offset, &head);
	if (out_sz != sizeof(ramd_head_t)) {
		return -2;
	}

	return 0;
}

int ramdump_get_used_size(void)
{
	struct device *nor_disk;
	int part_offset, part_size;
	ramd_head_t head;
	int out_sz;

	nor_disk = ramdump_get_device(&part_offset, &part_size);
	if (nor_disk == NULL){
		return 0;
	}

	// get ramdump header
	out_sz = ramdump_get_header(nor_disk, part_offset, &head);
	if (out_sz != sizeof(ramd_head_t)) {
		return 0;
	}

	return head.img_sz + out_sz;

}

int ramdump_data_traverse(uint8_t *buf, uint32_t len, int (*traverse_cb)(uint8_t *data, uint32_t max_len))
{
    uint32_t read_len;
    uint32_t read_addr;
    uint32_t total_len;
	struct device *nor_disk;
	int part_offset, part_size;

	nor_disk = ramdump_get_device(&part_offset, &part_size);
	if (nor_disk == NULL){
		printk("nor dev fail\n");
		return -1;
	}

    read_addr = part_offset;
    total_len = ramdump_get_used_size();

    while(total_len) {
		if(total_len > len){
			read_len = len;
		}else{
			read_len = total_len;
		}

        flash_read(nor_disk, read_addr, (void *)buf, read_len);

        read_addr += read_len;

        if(traverse_cb) {
            traverse_cb(buf, len);
        }

        total_len -= read_len;

#ifdef CONFIG_WATCHDOG
		watchdog_clear();
#endif

#ifdef CONFIG_TASK_WDT
		task_wdt_feed_all();
#endif

	}

    return part_size;
}


int ramdump_dump(void)
{
	struct device *nor_disk;
	int part_offset, part_size;
	ramd_head_t head;
	ramd_region_t region;
	int out_sz;

	nor_disk = ramdump_get_device(&part_offset, &part_size);
	if (nor_disk == NULL){
		printk("nor dev fail\n");
		return -1;
	}

	// get ramdump header
	out_sz = ramdump_get_header(nor_disk, part_offset, &head);
	if (out_sz != sizeof(ramd_head_t)) {
		return -2;
	}
	part_offset += sizeof(ramd_head_t);
	printk("[ramdump] org_sz=%d, img_sz=%d\n", head.org_sz, head.img_sz);

	while (head.img_sz > sizeof(ramd_region_t)) {
		// get ramdump region
		flash_read(nor_disk, part_offset, &region, sizeof(ramd_region_t));
		out_sz = sizeof(ramd_region_t) + region.img_sz;
		part_offset += out_sz;
		head.img_sz -= out_sz;
		printk("[region] mem_addr=0x%08x, mem_sz=%d, img_sz=%d\n",
			region.mem_addr, region.mem_sz, region.img_sz);
	}

	return 0;
}

int ramdump_check(void)
{
	struct device *nor_disk;
	int part_offset, part_size;
	ramd_head_t head;
	int out_sz;

	nor_disk = ramdump_get_device(&part_offset, &part_size);
	if (nor_disk == NULL){
		return -1;
	}

	// get ramdump header
	out_sz = ramdump_get_header(nor_disk, part_offset, &head);
	if (out_sz != sizeof(ramd_head_t)) {
		return -2;
	}

	return 0;
}



int ramdump_clear(void)
{
	struct device *nor_disk;
	int part_offset, part_size;

	nor_disk = ramdump_get_device(&part_offset, &part_size);
	if (nor_disk == NULL){
		return -1;
	}

	flash_erase(nor_disk, part_offset, part_size);

	return 0;
}


int ramdump_transfer(char *print_buf, int print_sz, int (*traverse_cb)(uint8_t *data, uint32_t max_len))
{
   int ret;
   int traverse_len, len;

   if(traverse_cb == NULL || print_buf == NULL)
	   return 0;

   /* Verify first to see if stored dump is valid */
   ret = ramdump_check();

   traverse_len = 0;
   if (ret != 0) {
	   traverse_len = sprintf(print_buf, "verification failed or no stored\r\n");
	   traverse_cb(print_buf, traverse_len);
	   goto out;
   }

   len = ramdump_data_traverse(print_buf, print_sz, traverse_cb);

   if(len > 0){
	   traverse_len += len;
   }else{
	   traverse_len = sprintf(print_buf, "traverse data failed %d\r\n", len);
	   traverse_cb(print_buf, traverse_len);
   }

out:
   return traverse_len;
}


/* Length of buffer of printable size */
#define LOG_BUF_SZ		64

/* Length of buffer of printable size plus null character */
#define LOG_BUF_SZ_RAW		(LOG_BUF_SZ + 1)

#define LOG_DATA_BUF_SZE  (256)
/**
 * @cond INTERNAL_HIDDEN
 *
 * These are for internal use only, so skip these in
 * public documentation.
 */

#define RAMDUMP_BEGIN_STR	"BEGIN#"
#define RAMDUMP_END_STR	"END#"
#define RAMDUMP_ERROR_STR	"ERROR CANNOT DUMP#"
#define RAMDUMP_CRC32_STR	"CRC32#"
#define RAMDUMP_BLOCK_CRC32_STR	"BLOCK#"
#define RAMDUMP_BLOCK_SIZE  (2048)
/*
 * Need to prefix ramdump strings to make it easier to parse
 * as log module adds its own prefixes.
 */
#define RAMDUMP_PREFIX_STR	"#CD:"

struct ramdump_print_ctx{
	int error;
	char data_buf[LOG_DATA_BUF_SZE];
	char log_buf[LOG_BUF_SZ_RAW];
	uint32_t crc_value;
	uint32_t crc_block_value;
	uint16_t item_cnt;
	uint16_t block_cnt;
};

static int hex2char(uint8_t x, char *c)
{
	if (x <= 9) {
		*c = x + '0';
	} else  if (x <= 15) {
		*c = x - 10 + 'a';
	} else {
		return -EINVAL;
	}

	return 0;
}

static void ramdump_print_start(void)
{
	printk("\r\n"RAMDUMP_PREFIX_STR RAMDUMP_BEGIN_STR"\r\n");
}

static void ramdump_print_end(struct ramdump_print_ctx *ctx)
{
	if (ctx->item_cnt != 0){
		printk(RAMDUMP_PREFIX_STR RAMDUMP_BLOCK_CRC32_STR"%d#" "0x%x \n", ctx->block_cnt, ctx->crc_block_value);
		k_busy_wait(1000);
	}

	if (ctx->error != 0) {
		printk(RAMDUMP_PREFIX_STR RAMDUMP_ERROR_STR"\r\n");
		k_busy_wait(1000);
	}

	printk(RAMDUMP_PREFIX_STR RAMDUMP_CRC32_STR "0x%x\n", ctx->crc_value);
	k_busy_wait(1000);
	printk(RAMDUMP_PREFIX_STR RAMDUMP_END_STR"\r\n");
}

void ramdump_print_buffer_output(uint8_t *buf, size_t buflen)
{
	uint8_t log_ptr = 0;
	size_t remaining = buflen;
	size_t i = 0;
	struct ramdump_print_ctx *ctx = (struct ramdump_print_ctx *)ram_get_unused();

	if ((buf == NULL) || (buflen == 0)) {
		ctx->error = -EINVAL;
		remaining = 0;
	}

	ctx->crc_value = utils_crc32(ctx->crc_value, buf, buflen);
	ctx->crc_block_value = utils_crc32(ctx->crc_block_value, buf, buflen);

	while (remaining > 0) {
		if (hex2char(buf[i] >> 4, &ctx->log_buf[log_ptr]) < 0) {
			printk("dump err %x\n", buf[i]);
			ctx->error = -EINVAL;
			break;
		}
		log_ptr++;

		if (hex2char(buf[i] & 0xf, &ctx->log_buf[log_ptr]) < 0) {
			printk("dump err %x\n", buf[i]);
			ctx->error = -EINVAL;
			break;
		}
		log_ptr++;

		i++;
		remaining--;

		if ((log_ptr >= LOG_BUF_SZ) || (remaining == 0)) {
			ctx->log_buf[log_ptr] = '\0';
			printk(RAMDUMP_PREFIX_STR "%s \n", (char *)(ctx->log_buf));
			log_ptr = 0;

			k_busy_wait(1000);

			ctx->item_cnt++;
			if(ctx->item_cnt == (RAMDUMP_BLOCK_SIZE / LOG_BUF_SZ)){
				printk(RAMDUMP_PREFIX_STR RAMDUMP_BLOCK_CRC32_STR"%d#" "0x%x \n", ctx->block_cnt, ctx->crc_block_value);
				ctx->block_cnt++;
				ctx->item_cnt = 0;
				ctx->crc_block_value = 0;
				k_busy_wait(1000);
			}
		}
	}
}

int ramdump_print_callback(uint8_t *data, uint32_t max_len)
{
	ramdump_print_buffer_output(data,  max_len);
	return 0;
}

int ramdump_print(void)
{
	struct ramdump_print_ctx *ctx = (struct ramdump_print_ctx *)ram_get_unused();

	memset(ctx, 0, sizeof(struct ramdump_print_ctx));

	ramdump_print_start();
	ramdump_transfer(ctx->data_buf, LOG_DATA_BUF_SZE, ramdump_print_callback);
	ramdump_print_end(ctx);

	return ctx->error;
}
