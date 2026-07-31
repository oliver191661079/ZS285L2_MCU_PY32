/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_defines.h"
#include "tool_app_inner.h"
#include <app_manager.h>
#include <audio_policy.h>
#include <audio_system.h>
#include <media_mem.h>
#include <media_player.h>
#include <tts_manager.h>
#include <shell/shell.h>

#ifdef CONFIG_SOC_DVFS
#include <soc_dvfs.h>
#endif

#define  STUB_CMD_DUMP_READ_PC_TOOL_STATUS (0x0100)
#define  STUB_CMD_DUMP_GET_MODESEL         (0xd000)
#define  STUB_CMD_DUMP_UPLOAD_DATA_NEW     (0xd100)
#define  STUB_CMD_DUMP_UPLOAD_DATA_OVER    (0x0500)

#define  PCM_DATA_EXCHANGE

#define LAZY_DELAY 1000
#define WORK_DELAY 200
#define BUSY_DELAY 1

#define DUMP_BUF_SIZE (1024)
#define MAX_DUMP (5)
#define MAX_DUMP_CURRENT (3)

#define STUB_BUF_SIZE ROUND_UP(STUB_COMMAND_HEAD_SIZE + 1 + (1024 + 1) * 2, 2)

static const uint8_t dump_tag_table[MAX_DUMP] = {
	MEDIA_DATA_TAG_EFFECT_IN,
	MEDIA_DATA_TAG_MIC1,
	MEDIA_DATA_TAG_EFFECT_OUT,
	MEDIA_DATA_TAG_DECODE_IN,
	MEDIA_DATA_TAG_DECODE_OUT1,
};

#define MAX_OPT_COUNT  10

typedef struct {
	uint8_t bSelArray[MAX_OPT_COUNT];
} dump_ST_ModuleSel;

struct DUMP_data {
	uint8_t in_btcall : 1;
	uint8_t last_in_btcall : 1;

	uint8_t run_status : 1;
	uint8_t last_run_status : 1;

	uint8_t upload_status : 1;
	uint8_t last_upload_status : 1;

	dump_ST_ModuleSel modesel;
	struct acts_ringbuf* dump_bufs[ARRAY_SIZE(dump_tag_table)];

	/* backstore memory for dump_bufs */
	char* dump_backstore;
	/* stub rw buffer */
	char* stub_buf;

	u32_t dump_cnt1;
	u32_t dump_cnt2;
	int dump_status;
};

struct DUMP_data* dump_data = NULL;
#ifdef PCM_DATA_EXCHANGE
static int dump_prepare_data_upload(struct DUMP_data* data);
static void dump_unprepare_data_upload(struct DUMP_data* data);
#endif

static char dump_tool_stub_buf[1548 * 2] ;

static struct DUMP_data* dump_data_alloc(void)
{
	struct DUMP_data* data = app_mem_malloc(sizeof(*data));
	if (!data)
		return NULL;

	memset(data, 0, sizeof(*data));

	data->stub_buf = dump_tool_stub_buf;
	data->dump_backstore = (char*)app_mem_malloc(DUMP_BUF_SIZE * MAX_DUMP_CURRENT);
	if (NULL == data->dump_backstore) {
		app_mem_free(data);
		ACT_LOG_ID_ERR(ALF_STR__dump_data_alloc__BACK_STORE_MALLOC_FA,
			0);
		return NULL;
	}

	return data;
}

static void dump_data_free(struct DUMP_data* data)
{
	if (NULL == data) {
		return;
	}

	if (NULL != data->dump_backstore) {
		app_mem_free(data->dump_backstore);
		data->dump_backstore = NULL;
	}

	data->stub_buf = NULL;
	app_mem_free(data);
}

static int dump_stub_read_data(int opcode, void* buf, unsigned int size)
{
	int ret = 0;

	ACT_LOG_ID_DBG(ALF_STR__dump_stub_read_data__OPCODE0XX, 1, opcode);
	ret = stub_get_data(tool_stub_dev_get(), opcode, buf, size);
	if (ret) {
		ACT_LOG_ID_WRN(ALF_STR__dump_stub_read_data__FAILED, 0);
	}

	return ret;
}

static int dump_stub_write_data(int opcode, void* buf, unsigned int size)
{
	int ret = 0;
	int err_cnt = 0;

retry:
	ACT_LOG_ID_DBG(ALF_STR__dump_stub_write_data__OPCODE0XX, 1, opcode);
	ret = stub_set_data(tool_stub_dev_get(), opcode, buf, size);
	if (ret) {
		ACT_LOG_ID_WRN(ALF_STR__dump_stub_write_data__FAILED_COUNTD, 1,
			err_cnt);
		if (++err_cnt < 10)
			goto retry;
	}

	return ret;
}

static int dump_stub_get_status(void)
{
	int32_t status = -1;
	dump_stub_read_data(STUB_CMD_DUMP_READ_PC_TOOL_STATUS, &status, 4);
	return status;
}

#ifdef PCM_DATA_EXCHANGE
static int dump_get_modsel(struct DUMP_data* data, dump_ST_ModuleSel* modsel)
{
	return dump_stub_read_data(STUB_CMD_DUMP_GET_MODESEL, modsel,
		sizeof(*modsel));
}

static int dump_prepare_data_upload(struct DUMP_data* data)
{
	char* dumpbuf = data->dump_backstore;
	int num_tags = 0;

	media_player_t* player = media_player_get_current_dumpable_player();
	if (!player) {
		ACT_LOG_ID_ERR(ALF_STR__dump_prepare_data_upload__NO_PLAYER, 0);
		return -EFAULT;
	}

	if (dump_get_modsel(data, &data->modesel)) {
		SYS_LOG_ERR("get_modsel failed");
		return -EIO;
	}
#if 0
	ACT_LOG_ID_DBG(ALF_STR__dump_prepare_data_upload__FORCELY_SELECT_DECOD,
			   0);
	data->modesel.bSelArray[0] = 1;
#endif

	for (int i = 0; i < ARRAY_SIZE(dump_tag_table); i++) {
		if (!data->modesel.bSelArray[i]) {
			ACT_LOG_ID_DBG(ALF_STR__dump_prepare_data_upload__POINT_D_IS_NOT_SELEC,
				1, i);
			continue;
		} else {
			ACT_LOG_ID_DBG(ALF_STR__dump_prepare_data_upload__POINT_D_IS_SELECTED,
				1, i);
		}

		if (num_tags++ >= MAX_DUMP_CURRENT) {
			SYS_LOG_ERR("exceed max %d tags", MAX_DUMP_CURRENT);
			return -EFAULT;
		}

		/* update to data tag */
		data->dump_bufs[i] = acts_ringbuf_init_ext(dumpbuf, DUMP_BUF_SIZE);
		if (!data->dump_bufs[i]) {
			ACT_LOG_ID_WRN(ALF_STR__dump_prepare_data_upload__NO_DUMP_DATA_AT_POIN,
				1, i);
			ACT_LOG_ID_ERR(
				ALF_STR__dump_prepare_data_upload__FAILED, 0);
			dump_unprepare_data_upload(data);
			return -ENOMEM;
		}

		dumpbuf += DUMP_BUF_SIZE;
		SYS_LOG_INF("start idx=%d, tag=%u, buf=%p", i,
			dump_tag_table[i], data->dump_bufs[i]);
	}

	return media_player_dump_data(player, ARRAY_SIZE(dump_tag_table),
		dump_tag_table, data->dump_bufs);
}

static void dump_unprepare_data_upload(struct DUMP_data* data)
{
	for (int i = 0; i < ARRAY_SIZE(data->dump_bufs); i++) {
		if (data->dump_bufs[i]) {
			SYS_LOG_INF("stop idx=%d, tag=%u, buf=%p", i,
				dump_tag_table[i], data->dump_bufs[i]);
			media_player_dump_data(NULL, 1, &dump_tag_table[i],
				NULL);
			acts_ringbuf_destroy_ext(data->dump_bufs[i]);
			data->dump_bufs[i] = NULL;
		}
	}
}

/* ASQT upload format*/
/* ------------------------------------------------------------------------------
 * | 1 bytes  | 1 bytes |  size   | 1 bytes |  size   | ... | 1 bytes |  size   |
 * ------------------------------------------------------------------------------
 * | num_tags |  tag_0  | data_0  |  tag_1  | data_1  | ... |  tag_n  | data_n  |
 * ------------------------------------------------------------------------------
 * data size is computed by formula:
 * 		size = (payload - 1 / num_tags) - 1
 * in which,
 * 		payload is the total size of the upload package.
 */
static int dump_upload_data_range(struct DUMP_data* data, int start, int end,
	int data_size)
{
	uint8_t* data_head = data->stub_buf + STUB_COMMAND_HEAD_SIZE;
	uint8_t* data_buff = data_head;
	uint8_t num_tags = 0, i;

	for (i = start; i < end; i++) {
		if (NULL == data->dump_bufs[i]) {
			// ACT_LOG_ID_DBG(ALF_STR__dump_upload_data_range__NO_DUMP_BUFFER_INDEX, 1, i);
			continue;
		}

		if (acts_ringbuf_length(data->dump_bufs[i]) < data_size) {
			return 0;
		}

		num_tags++;
	}

	if (num_tags == 0) {
		// ACT_LOG_ID_DBG(ALF_STR__dump_upload_data_range__NO_POINT_TO_UPLOAD, 0);
		return 0;
	}

	/* fill number of tags */
	*data_buff++ = num_tags;

	for (i = start; i < end; i++) {
		if (!data->dump_bufs[i])
			continue;

		/* fill tag */
		*data_buff++ = i;
		/* fill data */
		acts_ringbuf_get(data->dump_bufs[i], data_buff, data_size);
		data_buff += data_size;
	}

	return dump_stub_write_data(STUB_CMD_DUMP_UPLOAD_DATA_NEW,
		data->stub_buf,
		(unsigned int)(data_buff - data_head));
}

#define DATA_UNIT (128 * 4)

static int dump_upload_data(struct DUMP_data* data)
{
	int ret = 0;
	int len = DATA_UNIT;

	data->dump_cnt1++;
	ret = dump_upload_data_range(data, 0, 1, len);
	ret = dump_upload_data_range(data, 1, 2, len);
	ret = dump_upload_data_range(data, 2, 3, len);
	ret = dump_upload_data_range(data, 3, 4, len/2);
	ret = dump_upload_data_range(data, 4, 5, len);
	data->dump_cnt2++;
	return ret;
}
#endif

static int dump_btcall_is_on(void)
{
	if (!media_player_get_current_dumpable_player()) {
		SYS_LOG_DBG("no work player.\n");
		return 0;
	}

	return 1;
}

static int dump_init(void)
{
	SYS_LOG_INF("Enter\n");
#ifdef CONFIG_PLAYTTS
	tts_manager_lock();
#endif

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_set_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "tool");
#endif

	dump_data = dump_data_alloc();
	if (!dump_data) {
		ACT_LOG_ID_ERR(ALF_STR_tool_dump_loop___ASQT_DATA_ALLOC_FAI, 0);
		return -1;
	}

	return 0;
}

static void dump_exit(void)
{
#ifdef PCM_DATA_EXCHANGE
	dump_unprepare_data_upload(dump_data);
#endif

	if (NULL != dump_data) {
		dump_data_free(dump_data);
		dump_data = NULL;
	}

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_unset_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "tool");
#endif

#ifdef CONFIG_PLAYTTS
	tts_manager_unlock();
#endif

	SYS_LOG_INF("Exit");
}

static void dump_prepare_works(void)
{
	// btcall preparing work
	if (dump_data->in_btcall != dump_data->last_in_btcall) {
		dump_data->last_in_btcall = dump_data->in_btcall;
	}

	// Simulation preparing work
	if (dump_data->run_status != dump_data->last_run_status) {
		dump_data->last_run_status = dump_data->run_status;
	}

	// Upload preparing work
	if (dump_data->upload_status != dump_data->last_upload_status) {
		if (dump_data->upload_status) {
			SYS_LOG_INF("upload start");
#ifdef PCM_DATA_EXCHANGE
			dump_prepare_data_upload(dump_data);
#endif
		} else {
			SYS_LOG_INF("upload end");
#ifdef PCM_DATA_EXCHANGE
			dump_unprepare_data_upload(dump_data);
			dump_stub_write_data(STUB_CMD_DUMP_UPLOAD_DATA_OVER,
				NULL, 0);
#endif
		}

		dump_data->last_upload_status = dump_data->upload_status;
	}
}

void tool_dump_loop(void)
{
	int status = sNotReady;

	if (dump_init() != 0) {
		return;
	}

	while (!tool_is_quitting()) {
		/* get tool status */
		status = dump_stub_get_status();
		//SYS_LOG_INF("pc_tool_status:%d", status);
		dump_data->dump_status = status;
		if (status < 0) {
			os_sleep(LAZY_DELAY);
			continue;
		}

		switch (status) {
		case sUserStart:
			dump_data->run_status = 1;
			break;
		case sUserStop:
		case sReady:
			dump_data->run_status = 0;
			break;
		case sUserUpdate:
		default:
			break;
		}

		/* get btcall status */
		dump_data->in_btcall = dump_btcall_is_on();

		// set upload status
		dump_data->upload_status = (dump_data->run_status && dump_data->in_btcall);

		dump_prepare_works();

		// Download, Upload, and tool setting update.
		if (dump_data->upload_status) {
			// Need to feed and dumpout stream in time, so could not wait too long time.
#ifdef PCM_DATA_EXCHANGE
			for (int i = 0; i < WORK_DELAY / BUSY_DELAY; i++) {
				dump_upload_data(dump_data);
				//os_sleep(BUSY_DELAY);
			}
#else
			os_sleep(WORK_DELAY);
#endif
		} else {
			SYS_LOG_INF("Not upload.");
			os_sleep(LAZY_DELAY);
		}
	}

	dump_exit();
}

static int shell_dump_status(int argc, char *argv[])
{
	if (NULL != dump_data) {
		char *status = (char * )dump_data;
		printk("status:%02x\n", *status);
		printk("dump_cnt1:%d\n", dump_data->dump_cnt1);
		printk("dump_cnt2:%d\n", dump_data->dump_cnt2);
		printk("dump_status:%d\n", dump_data->dump_status);
		for(int i=0; i<MAX_DUMP; i++) {
			acts_ringbuf_dump(dump_data->dump_bufs[i], "D", "\t");
		}
	}
	return 0;
}

const struct shell_cmd dump_commands[] = {
	{ "stat", shell_dump_status, "dump status" },
	{ NULL, NULL, NULL }
};

SHELL_REGISTER("dump", dump_commands);