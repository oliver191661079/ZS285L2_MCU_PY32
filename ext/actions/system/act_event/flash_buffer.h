#ifndef __CONFIG_ACT_LOG_FLASH_BUFFER_H
#define __CONFIG_ACT_LOG_FLASH_BUFFER_H

#include <kernel.h>
#include <string.h>
#include <partition.h>
#include <cbuf.h>
#include <flash.h>
#include <act_event.h>

#define FLASH_WRITE_PAGE_SIZE (128)

struct flash_buffer_ctx
{
    /* For use with flash read/write */
    struct device *storage_dev;

    /* flash buffer base address */
    uint32_t base_addr;
    /* flash buffer total size, bytes unit */
    uint32_t total_size;
    /* flash buffer current write address, bytes unit */
    uint32_t log_start_addr;

    uint32_t log_end_addr;

    uint8_t init_flag :1;
    uint8_t erase_enable :1;
    uint8_t flush_enable :1;

    cbuf_t *rbuf;

	act_event_callback_t event_cb;
};

//flash buffer
struct flash_buffer_ctx *flash_buffer_init(int partition_id, int erase_enable, int flush_enable, cbuf_t *rbuf, act_event_callback_t event_cb);

int flash_buffer_sync(struct flash_buffer_ctx *ctx, struct flash_buffer_ctx *runtime_ctx);

int flash_buffer_write(struct flash_buffer_ctx *ctx, uint8_t *buf, uint32_t len);

int flash_buffer_get_data_size(struct flash_buffer_ctx *ctx);

int flash_buffer_read(struct flash_buffer_ctx *ctx, uint32_t addr, uint32_t *buf, uint32_t len);

int flash_buffer_clear(struct flash_buffer_ctx *ctx);

void flash_buffer_flush_enable(struct flash_buffer_ctx *ctx, int enable);

#endif
