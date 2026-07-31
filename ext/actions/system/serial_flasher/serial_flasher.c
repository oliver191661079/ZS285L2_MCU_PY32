/*
 * Copyright (c) 2022 Actions Semiconductor Co, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <device.h>
#include <thread_timer.h>
#include <misc/util.h>
#include <logging/sys_log.h>
#include <mem_manager.h>
#include <global_mem.h>
#include <msg_manager.h>
#include <bt_manager.h>
#include <srv_manager.h>
#include <sys_manager.h>
#include <sys_event.h>
#include <sys_wakelock.h>
#include <flash.h>
#include <partition.h>
#include <nvram_config.h>
#include <acts_ringbuf.h>

#include "serial_flasher_inner.h"
#include "serial_flasher.h"

typedef void (* ota_ui_cb_t)(u8_t val);

enum {
	SERIAL_FLASHER_REQUEST_UPGRADE,
	SERIAL_FLASHER_STATE_NEGOTIATION,
	SERIAL_FLASHER_STATE_BURN_IMAGE,
	SERIAL_FLASHER_STATE_END,
};

struct mcu_ota_breakpoint
{
	uint8_t need_upgrade;
	uint8_t ota_type;		// 0-udisk, 1-app
	uint32_t file_size;
}__packed;


static struct serial_flasher *serial_flasher_ctx;


static struct serial_flasher *serial_flasher_ctx_init(void)
{
    if(serial_flasher_ctx){
        return serial_flasher_ctx;
    }

    serial_flasher_ctx = mem_malloc(sizeof(struct serial_flasher));

    if(!serial_flasher_ctx){
        return NULL;
    }

	memset(serial_flasher_ctx, 0, sizeof(struct serial_flasher));

    return serial_flasher_ctx;
}

static int serial_flasher_ctx_deinit(struct serial_flasher *ctx)
{
    if(!ctx){
        return 0;
    }

    mem_free(ctx);

	return 0;
}

struct serial_flasher *serial_flasher_get_ctx(void)
{
    return serial_flasher_ctx;
}

int serial_flasher_wait_flash_end(struct serial_flasher *ctx)
{
	int err = 0;
	int wait_timeout  = 0;

	SYS_LOG_INF("wait, %d\n", ctx->upgrade_end);

	while (ctx->upgrade_end == 0) {
		err = serial_flasher_process_command(ctx, NULL);
		if (err){
			wait_timeout += ctx->read_timeout;
			if(wait_timeout >= (ctx->host_wait_timeout * 1000)){
				SYS_LOG_ERR("wait flash end timeout %d\n", wait_timeout);
				return -EIO;
			}
		}else{
			wait_timeout = 0;
		}
	}

	SYS_LOG_INF("wait ctx %p, return %d result %d %d\n", ctx, err, ctx->upgrade_result, ctx->upgrade_end);

	return err;
}

int serial_flasher_binary_file_read(struct serial_flasher *ctx, uint32_t offset, uint8_t *buf, uint32_t size)
{
	int err;
	int progress;

	progress = (offset + size) * 100 / ctx->mcu_file_size;

	if(ctx->progress != progress){
		ctx->progress = progress;
		SYS_LOG_INF("offset 0x%x, size %d, progress %d%%", offset, size, progress);
		if(ctx->cb)
				ctx->cb(SERIAL_FLASHER_UPDATE_PROCESS, progress);
	}

	err = flash_read(ctx->flash_dev, ctx->part->offset + offset, buf, size);
	if (err < 0) {
		SYS_LOG_INF("read data err %d", err);
		return -EIO;
	}

	if (offset == 0){
		if(buf[0] != 0x41 && buf[1] != 0x4F \
			&& buf[2] != 0x54 && buf[3] != 0x41){
			buf[0] = 0x41;
			buf[1] = 0x4F;
			buf[2] = 0x54;
			buf[3] = 0x41;
		}
	}

	return 0;
}

int serial_flasher_binary_file_init(struct serial_flasher *ctx, const char *flash_dev_name, int file_size)
{
	ctx->part = partition_get_part(PARTITION_FILE_ID_OTA_TEMP);

	if (!ctx->part) {
		SYS_LOG_ERR("stream create failed \n");
		return -EIO;
	}

	ctx->flash_dev = device_get_binding(flash_dev_name);
	if (!ctx->flash_dev) {
		SYS_LOG_ERR("cannot found storage device %s", flash_dev_name);
		return -EINVAL;
	}

	ctx->binary_read_callback = serial_flasher_binary_file_read;

	ctx->mcu_file_size = file_size;

	ctx->stream_opened = 1;

	SYS_LOG_INF("open stream %p", ctx->part);

	return 0;

}


int serial_flasher_binary_file_deinit(struct serial_flasher *ctx)
{
	ctx->stream_opened = 0;

	return 0;
}

/* type: 0-udisk, 1-app */
int serial_flasher_mcu_upgrade_breakpoint_save(uint8_t need_upgrade, uint8_t type, uint32_t file_size)
{
	int err;
	struct mcu_ota_breakpoint mcu_bp;

	mcu_bp.need_upgrade = need_upgrade;
	mcu_bp.ota_type = type;
	mcu_bp.file_size = file_size;

	SYS_LOG_INF("save mcu bp: state %d, type %d, size %x\n", need_upgrade, type, file_size);

	err = nvram_config_set("MCU_OTA_BP", &mcu_bp, sizeof(mcu_bp));
	if (err) {
		SYS_LOG_ERR("---set MCU_OTA_BP fail!\n");
		return -1;
	}

	return 0;
}

int serial_flasher_is_mcu_need_upgrade(void)
{
	int rlen;
	struct mcu_ota_breakpoint mcu_bp;

	rlen = nvram_config_get("MCU_OTA_BP", &mcu_bp, sizeof(mcu_bp));
	if (rlen != sizeof(mcu_bp)) {
		SYS_LOG_INF("cannot found MCU_OTA_BP");
		return 0;
	}

	SYS_LOG_INF("load bp: state %d", mcu_bp.need_upgrade);

	return mcu_bp.need_upgrade;
}

int serial_flasher_get_mcu_file_size(void)
{
	int rlen;
	struct mcu_ota_breakpoint mcu_bp;

	rlen = nvram_config_get("MCU_OTA_BP", &mcu_bp, sizeof(mcu_bp));
	if (rlen != sizeof(mcu_bp)) {
		SYS_LOG_INF("cannot found MCU_OTA_BP");
		return 0;
	}

	SYS_LOG_INF("load bp: size %x", mcu_bp.file_size);

	return mcu_bp.file_size;
}


int serial_flasher_is_sdcard_upgrade(void)
{
	int rlen;
	struct mcu_ota_breakpoint mcu_bp;

	rlen = nvram_config_get("MCU_OTA_BP", &mcu_bp, sizeof(mcu_bp));
	if (rlen != sizeof(mcu_bp)) {
		SYS_LOG_INF("cannot found MCU_OTA_BP");
		return 0;
	}

	SYS_LOG_INF("load bp: ota_type %d", mcu_bp.ota_type);

	if(mcu_bp.ota_type == 0)
		return 1;

	return 0;
}

struct serial_flasher *serial_flasher_init(const char *flash_dev_name, serial_flasher_cb_t cb, void (*send_cb)(uint8_t *buf, uint32_t len), int file_size)
{
    int ret_val;

	struct serial_flasher *ctx;
    ctx = serial_flasher_ctx_init();
	ctx->cb = cb;
	os_sem_init(&ctx->read_sem, 0, UINT_MAX);

    ret_val = serial_flasher_port_init(ctx, CONFIG_SERIAL_FLASHER_ON_DEV_NAME, 512, send_cb);

    if(ret_val){
		serial_flasher_ctx_deinit(ctx);
        SYS_LOG_ERR("serial port init error\n");
        return NULL;
    }

	ret_val = serial_flasher_binary_file_init(ctx, flash_dev_name, file_size);

    if(ret_val){
		serial_flasher_port_deinit(ctx);
		serial_flasher_ctx_deinit(ctx);
        SYS_LOG_ERR("binary file init error\n");
        return NULL;
    }

	return ctx;

}

int serial_flasher_deinit(struct serial_flasher *ctx)
{
	serial_flasher_port_deinit(ctx);
	serial_flasher_ctx_deinit(ctx);

	return 0;
}

int serial_flasher_save_rx_data(struct serial_flasher *ctx, uint8_t *data, uint32_t len)
{
	int ret_val;
	if(!ctx || !ctx->cbuf){
		return -EINVAL;
	}

	ret_val = acts_ringbuf_put(ctx->cbuf, data, len);

	os_sem_give(&ctx->read_sem);

	return ret_val;
}

int serial_flasher_routinue(struct serial_flasher *ctx)
{
    int ret_val;
	int serial_flasher_state;
	int serial_flasher_end;

	SYS_LOG_INF("enter");

	serial_flasher_state = SERIAL_FLASHER_REQUEST_UPGRADE;

	serial_flasher_end = false;

	while(!serial_flasher_end){
		SYS_LOG_INF("-------serial_flasher_state: %d", serial_flasher_state);
		switch(serial_flasher_state){
			case SERIAL_FLASHER_REQUEST_UPGRADE:
		    ret_val = serial_flasher_request_upgrade(ctx);

		    if(ret_val){
				serial_flasher_state = SERIAL_FLASHER_STATE_END;
		    }else{
				serial_flasher_state = SERIAL_FLASHER_STATE_NEGOTIATION;
			}
			break;

			case SERIAL_FLASHER_STATE_NEGOTIATION:
		    ret_val = serial_flasher_connect_negotiation(ctx);
		    if(ret_val){
		        SYS_LOG_ERR("connect negotiation failed\n");
		        serial_flasher_state = SERIAL_FLASHER_REQUEST_UPGRADE;
		    }else{
				serial_flasher_send_negotiation_result(ctx, true);
				serial_flasher_state = SERIAL_FLASHER_STATE_BURN_IMAGE;
			}
			break;

			case SERIAL_FLASHER_STATE_BURN_IMAGE:
			ret_val = serial_flasher_wait_flash_end(ctx);
			if(ret_val){
				//serial_flasher_state = SERIAL_FLASHER_REQUEST_UPGRADE;
				serial_flasher_state = SERIAL_FLASHER_STATE_END;
			}else{
				serial_flasher_state = SERIAL_FLASHER_STATE_END;
			}
			break;

			case SERIAL_FLASHER_STATE_END:
			if(ctx->upgrade_result == true){
				SYS_LOG_INF("wait device restart %d s\n", ctx->device_restart_timeout);
				serial_flasher_mcu_upgrade_breakpoint_save(false, 0, 0);
				//os_msleep(ctx->device_restart_timeout * 1000);
			}else{
				ret_val=serial_flasher_cancel_upgrade(ctx);
				ret_val = -EIO;
			}
			serial_flasher_binary_file_deinit(ctx);
			serial_flasher_end = true;
			break;
		}
	}

	return ret_val;
}



