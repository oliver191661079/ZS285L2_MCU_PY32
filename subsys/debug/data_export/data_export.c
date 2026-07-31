/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file data_export.c
 */
#include <stdio.h>
#include <string.h>
#include <zephyr/types.h>
#include <linker/linker-defs.h>
#include <errno.h>
#include <init.h>
#include <device.h>
#include <debug/data_export.h>
#include <mem_manager.h>
#include <crc.h>
#ifdef CONFIG_WATCHDOG
#include <watchdog_hal.h>
#endif

#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif

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

#define DATAEXPORT_BEGIN_STR	"BEGIN#"
#define DATAEXPORT_END_STR		"END#"
#define DATAEXPORT_ERROR_STR	"ERROR CANNOT DUMP#"
#define DATAEXPORT_CRC32_STR	"CRC32#"
#define DATAEXPORT_BLOCK_CRC32_STR	"BLOCK#"
#define DATAEXPORT_BLOCK_SIZE  (2048)
/*
 * Need to prefix data_export strings to make it easier to parse
 * as log module adds its own prefixes.
 */
#define DATAEXPORT_PREFIX_STR	"#DA:"

struct data_export_print_ctx{
	int error;
	char *data_buf;
	uint32_t data_len;
	char log_buf[LOG_BUF_SZ_RAW];
	uint32_t crc_value;
	uint32_t crc_block_value;
	uint16_t item_cnt;
	uint16_t block_cnt;
};

extern int _arch_exception_vector_read(void);

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

static void data_export_print_start(struct data_export_print_ctx *ctx)
{
	printk("\r\n"DATAEXPORT_PREFIX_STR DATAEXPORT_BEGIN_STR"0x%x \n", (uint32_t)ctx->data_buf);
}

static void data_export_print_end(struct data_export_print_ctx *ctx)
{
	if (ctx->item_cnt != 0){
		printk(DATAEXPORT_PREFIX_STR DATAEXPORT_BLOCK_CRC32_STR"%d#" "0x%x \n", ctx->block_cnt, ctx->crc_block_value);
		k_busy_wait(1000);
	}

	if (ctx->error != 0) {
		printk(DATAEXPORT_PREFIX_STR DATAEXPORT_ERROR_STR"\r\n");
		k_busy_wait(1000);
	}

	printk(DATAEXPORT_PREFIX_STR DATAEXPORT_CRC32_STR "0x%x\n", ctx->crc_value);
	k_busy_wait(1000);
	printk(DATAEXPORT_PREFIX_STR DATAEXPORT_END_STR"\r\n");
}

void data_export_delay(void)
{
	if(!k_is_in_isr() && _arch_exception_vector_read() > 0){
		k_busy_wait(1000);
	}else{
		k_sleep(1);
	}

#ifdef CONFIG_WATCHDOG
	watchdog_clear();
#endif

#ifdef CONFIG_TASK_WDT
	task_wdt_feed_all();
#endif
}

int data_export_print_buffer_output(struct data_export_print_ctx *ctx, uint8_t *buf, uint32_t buflen)
{
	uint8_t log_ptr = 0;
	size_t remaining = buflen;
	size_t i = 0;

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
			printk(DATAEXPORT_PREFIX_STR "%s \n", (char *)(ctx->log_buf));
			log_ptr = 0;

			data_export_delay();

			ctx->item_cnt++;
			if(ctx->item_cnt == (DATAEXPORT_BLOCK_SIZE / LOG_BUF_SZ)){
				printk(DATAEXPORT_PREFIX_STR DATAEXPORT_BLOCK_CRC32_STR"%d#" "0x%x \n", ctx->block_cnt, ctx->crc_block_value);
				ctx->block_cnt++;
				ctx->item_cnt = 0;
				ctx->crc_block_value = 0;
				data_export_delay();
			}
		}
	}

	return 0;
}

int data_export_traverse(struct data_export_print_ctx *ctx, uint32_t len, int (*traverse_cb)(struct data_export_print_ctx *ctx, uint8_t *data, uint32_t max_len))
{
    uint32_t read_len;
    uint8_t *read_ptr;
    uint32_t total_len;

    read_ptr = ctx->data_buf;
    total_len = ctx->data_len;

    while(total_len) {
		if(total_len > len){
			read_len = len;
		}else{
			read_len = total_len;
		}

        if(traverse_cb) {
            traverse_cb(ctx, read_ptr, len);
        }

#ifdef CONFIG_WATCHDOG
		watchdog_clear();
#endif

#ifdef CONFIG_TASK_WDT
		task_wdt_feed_all();
#endif

        total_len -= read_len;

		read_ptr += read_len;
	}

    return ctx->data_len;
}

int data_export_print(uint8_t *buf, size_t buflen)
{
	struct data_export_print_ctx inner_ctx;
	struct data_export_print_ctx *ctx = &inner_ctx;

	memset(ctx, 0, sizeof(struct data_export_print_ctx));
	ctx->data_buf = buf;
	ctx->data_len = buflen;

	data_export_print_start(ctx);
	data_export_traverse(ctx, 256, data_export_print_buffer_output);
	data_export_print_end(ctx);

	return ctx->error;
}
