/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <init.h>
#include <device.h>
#include <string.h>
#include <disk_access.h>
#include <flash.h>
#include <gpio.h>
#include <misc/byteorder.h>
#include <mem_manager.h>
#include <mmc/mmc.h>
#include <mmc/sd.h>
#include <board.h>
#include "mmc_ops.h"

#define SYS_LOG_DOMAIN "sd"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_MMC_LEVEL
#include <logging/sys_log.h>

#define MMC_CMD_RETRIES					2

#define SD_CARD_RW_MAX_SECTOR_CNT_PROTOCOL	(65536)
#define SD_CARD_INVALID_OFFSET          (-1)
#define SD_CARD_RW_MAX_SECTOR_CNT		(32)

#define SD_CARD_SECTOR_SIZE			    (512)
#define SD_CARD_INIT_CLOCK_FREQ			(100000)
#define SD_CARD_SDR12_CLOCK_FREQ		(25000000)
#define SD_CARD_SDR25_CLOCK_FREQ		(50000000)

#define SD_CARD_WAITBUSY_TIMEOUT_MS		(100)

/* card type */
enum {
	CARD_TYPE_SD = 0,
	CARD_TYPE_MMC = 1,
};

struct mmc_csd {
	u8_t	ccs;
	u8_t	sd_spec;
	u8_t	suport_cmd23;
	u32_t	sector_count;
	u32_t	sector_size;
};

/* eMMC specific csd and ext_csd all in this structure */
struct mmc_ext_csd {
	/* emmc ext_csd */
	u8_t	rev;

	/* emmc csd */
	u8_t	mmca_vsn;
	u32_t	erase_size;	/* erase size in sectors */
};

struct sd_card_data {
	struct device *mmc_dev;
	struct k_sem lock;

	u8_t card_type;

	bool card_initialized;
	bool force_plug_out;
	bool is_low_power;
	u32_t rca;
	struct mmc_csd mmc_csd;		/* card specific */
	struct mmc_ext_csd ext_csd;	/* mmc v4 extended card specific */

	struct mmc_cmd cmd; /* use the data segment for reducing stack consumption */

	struct device *detect_gpio_dev;

#ifdef CONFIG_MMC_SDCARD_SHOW_PERF
	u32_t max_rd_use_time; /* record the max read use time */
	u32_t max_wr_use_time; /* record the max write use time */
	u32_t rec_perf_timestamp; /* record the timestamp for showing performance */
	u32_t rec_perf_rd_size; /* record the read total size for showing performance */
	u32_t rec_perf_wr_size; /* record the read total size for showing performance */
#endif
	u32_t next_rw_offs; /* record the next read/write block offset for high performance mode */
	u8_t on_high_performance : 1; /* indicates that sdcard works on high performance mode */
	u8_t prev_is_write : 1; /* record the previous command is read or write for high performance mode */
};

static int sd_card_storage_init(struct device *dev);

#define UNSTUFF_BITS(resp,start,size)					\
	({								\
		const int __size = size;				\
		const u32_t __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);			\
		const int __shft = (start) & 31;			\
		u32_t __res;						\
									\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})

static int mmc_decode_csd(struct sd_card_data *sd, u32_t *resp)
{
	struct mmc_csd *csd = &sd->mmc_csd;
	u32_t c_size, c_size_mult, capacity;
	int csd_struct;

	csd_struct = UNSTUFF_BITS(resp, 126, 2);

	switch (csd_struct) {
	case 0:
		c_size_mult = UNSTUFF_BITS(resp, 47, 3);
		c_size = UNSTUFF_BITS(resp, 62, 12);

		csd->sector_size = 1 << UNSTUFF_BITS(resp, 80, 4);
		csd->sector_count = (1 + c_size) << (c_size_mult + 2);

		capacity = csd->sector_size * csd->sector_count / 1024 / 1024;
		break;
	case 1:
		/* c_size: 512KB block count */
		c_size = UNSTUFF_BITS(resp, 48, 22);
		csd->sector_size = 512;
		csd->sector_count = (c_size + 1) * 1024;

		capacity = (1 + c_size) / 2;
		break;
	default:
		SYS_LOG_ERR("unknown csd version %d", csd_struct);
		return -EINVAL;
	}

	SYS_LOG_INF("CSD: capacity %u MB", capacity);

	return 0;
}

static int emmc_decode_csd(struct sd_card_data *sd, u32_t *resp)
{
	struct mmc_csd *csd = &sd->mmc_csd;
	u32_t c_size, c_size_mult, capacity;
	u8_t write_blkbits;
	int csd_struct;

	/*
	 * We only understand CSD structure v1.1 and v1.2.
	 * v1.2 has extra information in bits 15, 11 and 10.
	 * We also support eMMC v4.4 & v4.41.
	 */
	csd_struct = UNSTUFF_BITS(resp, 126, 2);
	if (csd_struct == 0) {
		SYS_LOG_ERR("unrecognised CSD structure version %d\n", csd_struct);
		return -EINVAL;
	}

	sd->ext_csd.mmca_vsn = UNSTUFF_BITS(resp, 122, 4);

	write_blkbits = UNSTUFF_BITS(resp, 22, 4);
	if (write_blkbits >= 9) {
		u8_t a = UNSTUFF_BITS(resp, 42, 5);
		u8_t b = UNSTUFF_BITS(resp, 37, 5);
		sd->ext_csd.erase_size = (a + 1) * (b + 1);
		sd->ext_csd.erase_size <<= write_blkbits - 9;
	}

	c_size_mult = UNSTUFF_BITS(resp, 47, 3);
	c_size = UNSTUFF_BITS(resp, 62, 12);

	csd->sector_size = 1 << UNSTUFF_BITS(resp, 80, 4);
	csd->sector_count = (1 + c_size) << (c_size_mult + 2);

	capacity = csd->sector_size * csd->sector_count / 1024 / 1024;

	SYS_LOG_INF("CSD: capacity %u MB", capacity);

	return 0;
}

/*
 * Decode extended CSD.
 */
static int emmc_read_ext_csd(struct sd_card_data *sd, u8_t *ext_csd)
{
	u32_t sectors = 0;
	u32_t data_sector_size = 512;

	sd->ext_csd.rev = ext_csd[EXT_CSD_REV];
	if (sd->ext_csd.rev > 8) {
		SYS_LOG_ERR("unrecognised EXT_CSD revision %d\n", sd->ext_csd.rev);
		return -EINVAL;
	}

	sd->ext_csd.rev = ext_csd[EXT_CSD_REV];

	if (sd->ext_csd.rev >= 3) {
		if (ext_csd[EXT_CSD_ERASE_GROUP_DEF] & 1)
			sd->ext_csd.erase_size = ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] << 10;
	}

	if (sd->ext_csd.rev >= 2) {
		sectors =
				ext_csd[EXT_CSD_SEC_CNT + 0] << 0 |
				ext_csd[EXT_CSD_SEC_CNT + 1] << 8 |
				ext_csd[EXT_CSD_SEC_CNT + 2] << 16 |
				ext_csd[EXT_CSD_SEC_CNT + 3] << 24;
	} else {
		sectors = 0;
	}

	/* eMMC v4.5 or later */
	if (sd->ext_csd.rev >= 6) {
		if (ext_csd[EXT_CSD_DATA_SECTOR_SIZE] == 1)
			data_sector_size = 4096;
		else
			data_sector_size = 512;
	} else {
		data_sector_size = 512;
	}

	SYS_LOG_INF("EXT_CSD: rev %d, sector_cnt %u, sector_size %u, erase_size %u",
			sd->ext_csd.rev, sectors, data_sector_size, sd->ext_csd.erase_size);

	/* FIXME: copy to mmc_csd */
	if (sectors > 0) {
		sd->mmc_csd.sector_count = sectors;
		sd->mmc_csd.sector_size = data_sector_size;
	}

	SYS_LOG_INF("EXT_CSD: capacity %u MB",
		sd->mmc_csd.sector_count * (sd->mmc_csd.sector_size / 512) / 2048);

	return 0;
}

static void mmc_decode_scr(struct sd_card_data *sd, u32_t *scr)
{
	struct mmc_csd *csd = &sd->mmc_csd;
	u32_t resp[4];

	resp[3] = scr[1];
	resp[2] = scr[0];

	csd->sd_spec = UNSTUFF_BITS(resp, 56, 4);
	if (csd->sd_spec == SCR_SPEC_VER_2) {
		/* Check if Physical Layer Spec v3.0 is supported */
		if (UNSTUFF_BITS(resp, 47, 1)) {
			csd->sd_spec = 3;
		}
	}

	/* check Set Block Count command */
	csd->suport_cmd23 = !!UNSTUFF_BITS(resp, 33, 1);

	SYS_LOG_INF("SCR: sd_spec %d, suport_cmd23 %d, bus_width 0x%x",
		    csd->sd_spec, csd->suport_cmd23,
		    UNSTUFF_BITS(resp, 48, 4));
}

static int mmc_send_if_cond(struct device *mmc_dev, struct mmc_cmd *cmd, u32_t ocr)
{
	static const u8_t test_pattern = 0xAA;
	u8_t result_pattern;
	int ret;

	memset(cmd, 0, sizeof(struct mmc_cmd));

	cmd->opcode = SD_SEND_IF_COND;
	cmd->arg = ((ocr & 0xFF8000) != 0) << 8 | test_pattern;
	cmd->flags = MMC_RSP_R7 | MMC_CMD_BCR;

	ret = mmc_send_cmd(mmc_dev, cmd);

	result_pattern = cmd->resp[0] & 0xFF;

	if (result_pattern != test_pattern)
		return -EIO;

	return 0;
}

static int mmc_send_app_op_cond(struct device *mmc_dev, struct mmc_cmd *cmd, u32_t ocr, u32_t *rocr)
{
	int i, err = 0;

	memset(cmd, 0, sizeof(struct mmc_cmd));

	cmd->opcode = SD_APP_OP_COND;
	cmd->arg = ocr;
	cmd->flags = MMC_RSP_R3 | MMC_CMD_BCR;

	for (i = 50; i; i--) {
		err = mmc_send_app_cmd(mmc_dev, 0, cmd, MMC_CMD_RETRIES);
		if (err)
			break;

		/* if we're just probing, do a single pass */
		if (ocr == 0)
			break;

		/* otherwise wait until reset completes */
		if (cmd->resp[0] & MMC_CARD_BUSY)
			break;

		err = -ETIMEDOUT;

		k_sleep(20);
	}

	if (rocr)
		*rocr = cmd->resp[0];

	return err;
}

static int emmc_send_app_op_cond(struct device *mmc_dev, struct mmc_cmd *cmd, u32_t ocr, u32_t *rocr)
{
	int i, err = 0;

	memset(cmd, 0, sizeof(struct mmc_cmd));

	cmd->opcode = MMC_SEND_OP_COND;
	cmd->arg = ocr;
	cmd->flags = MMC_RSP_R3 | MMC_CMD_BCR;

	/* There are some eMMC need more time to power up */
	for (i = 100; i; i--) {
		err = mmc_send_cmd(mmc_dev, cmd);
		if (err)
			break;

		/* if we're just probing, do a single pass */
		if (ocr == 0)
			break;

		/* otherwise wait until reset completes */
		if (cmd->resp[0] & MMC_CARD_BUSY)
			break;

		err = -ETIMEDOUT;

		k_sleep(20);
	}

	SYS_LOG_DBG("Use %d circulations to power up", i);

	if (rocr)
		*rocr = cmd->resp[0];

	return err;
}

static int mmc_sd_switch(struct device *mmc_dev, int mode, int group,
	u8_t value, u8_t *resp)
{
	int err;
	struct mmc_cmd cmd = {0};

	mode = !!mode;
	value &= 0xF;

	cmd.opcode = SD_SWITCH;
	cmd.arg = mode << 31 | 0x00FFFFFF;
	cmd.arg &= ~(0xF << (group * 4));
	cmd.arg |= value << (group * 4);
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC | MMC_DATA_READ;
	cmd.blk_size = 64;
	cmd.blk_num = 1;
	cmd.buf = resp;

	err = mmc_send_cmd(mmc_dev, &cmd);
	if (err)
		return err;

	return 0;
}

static int emmc_switch(struct device *mmc_dev, u8_t set, u8_t index, u8_t value)
{
	struct mmc_cmd cmd = {0};

	cmd.opcode = MMC_SWITCH;
	cmd.flags = MMC_RSP_R1B | MMC_CMD_AC;
	cmd.arg = (0x03 << 24) |
				 (index << 16) |
				 (value << 8) | set;

	return mmc_send_cmd(mmc_dev, &cmd);
}

/*
 * Test if the card supports high-speed mode and, if so, switch to it.
 */
int mmc_sd_switch_hs(struct device *mmc_dev)
{
	u32_t cap;
	int err;
	u8_t status[64];

	cap = mmc_get_capability(mmc_dev);
	if (!(cap & MMC_CAP_SD_HIGHSPEED))
		return 0;

	err = mmc_sd_switch(mmc_dev, 1, 0, 1, status);
	if (err)
		goto out;

	if ((status[16] & 0xF) != 1) {
		SYS_LOG_WRN("Failed to switch card to high-speed mode");
		err = 0;
	} else {
		err = 1;
	}
out:
	return err;
}

int emmc_switch_hs(struct device *mmc_dev)
{
	int err;
	err = emmc_switch(mmc_dev, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 1);
	if (err)
		return 0;

	return 1;
}

static int mmc_app_set_bus_width(struct device *mmc_dev, struct mmc_cmd *cmd, int rca, int width)
{
	int err;

	memset(cmd, 0, sizeof(struct mmc_cmd));

	cmd->opcode = SD_APP_SET_BUS_WIDTH;
	cmd->flags = MMC_RSP_R1 | MMC_CMD_AC;

	switch (width) {
	case MMC_BUS_WIDTH_1:
		cmd->arg = SD_BUS_WIDTH_1;
		break;
	case MMC_BUS_WIDTH_4:
		cmd->arg = SD_BUS_WIDTH_4;
		break;
	default:
		return -EINVAL;
	}

	err = mmc_send_app_cmd(mmc_dev, rca, cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	return 0;
}

static int mmc_app_send_scr(struct device *mmc_dev, struct mmc_cmd *cmd, int rca, u32_t *scr)
{
	int err;
	u32_t tmp_scr[2];

	memset(cmd, 0, sizeof(struct mmc_cmd));

	cmd->opcode = SD_APP_SEND_SCR;
	cmd->arg = 0;
	cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC | MMC_DATA_READ;
	cmd->blk_size = 8;
	cmd->blk_num = 1;
	cmd->buf = (u8_t *)tmp_scr;

	err = mmc_send_app_cmd(mmc_dev, rca, cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	scr[0] = sys_be32_to_cpu(tmp_scr[0]);
	scr[1] = sys_be32_to_cpu(tmp_scr[1]);

	return 0;
}

#ifdef CONFIG_MMC_SDCARD_LOW_POWER

#ifdef CONFIG_MMC_SDCARD_LOW_POWER_SLEEP
static int mmc_can_sleep(struct sd_card_data *sd)
{
	return (sd->card_type == CARD_TYPE_MMC && sd->ext_csd.rev >= 3);
}

static int mmc_sleep_awake(struct sd_card_data *sd, int is_sleep)
{
	int err;
	struct mmc_cmd *cmd = &sd->cmd;

	memset(cmd, 0, sizeof(struct mmc_cmd));

	cmd->opcode = MMC_SLEEP_AWAKE;
	cmd->flags = MMC_RSP_R1B | MMC_CMD_AC;
	cmd->arg = sd->rca << 16;
	if (is_sleep) {
		cmd.arg |= (1 << 15);
	}

	err = mmc_send_cmd(sd->mmc_dev, cmd);
	if (err) {
		SYS_LOG_ERR("sleep/wakeup failed, err %d", err);
		return err;
	}

	return 0;
}
#endif	/* CONFIG_MMC_SDCARD_LOW_POWER_SLEEP */

static int mmc_enter_low_power(struct sd_card_data *sd)
{
	int err;

	sd->is_low_power = true;

	err = mmc_select_card(sd->mmc_dev, &sd->cmd, 0);
	if (err)
		return err;

#ifdef CONFIG_MMC_SDCARD_LOW_POWER_SLEEP
	if (!mmc_can_sleep(sd))
		return 0;

	err = mmc_sleep_awake(sd, 1);
#endif
	return err;
}

static int mmc_exit_low_power(struct sd_card_data *sd)
{
	int err;

	if (!sd->is_low_power)
		return 0;

	sd->is_low_power = false;

#ifdef CONFIG_MMC_SDCARD_LOW_POWER_SLEEP
	if (mmc_can_sleep(sd)) {
		err = mmc_sleep_awake(sd, 0);
		if (err)
			return err;
	}
#endif

	err = mmc_select_card(sd->mmc_dev, &sd->cmd, sd->rca);

	return err;
}
#endif /* CONFIG_MMC_SDCARD_LOW_POWER */

static int get_card_status(struct sd_card_data *sd, u32_t *status)
{
	int err;

	err = mmc_send_status(sd->mmc_dev, &sd->cmd, sd->rca, status);
#if 0
	if (err) {
		SYS_LOG_ERR("failed to get card status");
	}
#endif

	return err;
}

int mmc_sigle_blk_rw(struct device *dev, int is_write, unsigned int addr,
		     void *dst, int blk_size)
{
	struct sd_card_data *sd = dev->driver_data;
	struct device *mmc_dev = sd->mmc_dev;
	struct mmc_cmd *cmd = &sd->cmd;
	int err;

	memset(cmd, 0, sizeof(struct mmc_cmd));

	/* When transmission changed from multi-block to single block
	 * need to send stop command on high performance mode.
	 */
	if (sd->on_high_performance && (sd->next_rw_offs != SD_CARD_INVALID_OFFSET)) {
		SYS_LOG_DBG("%d next_rw_offs %d", __LINE__, sd->next_rw_offs);
		sd->next_rw_offs = SD_CARD_INVALID_OFFSET;
		err = mmc_stop_block_transmission(mmc_dev, cmd);
		if (err) {
			SYS_LOG_ERR("mmc stop block transmission failed, ret %d", err);
			return err;
		}
	}

	if (is_write) {
		cmd->opcode = MMC_WRITE_BLOCK;
		cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC | MMC_DATA_WRITE;
	} else {
		cmd->opcode = MMC_READ_SINGLE_BLOCK;
		cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC | MMC_DATA_READ;
	}

	cmd->arg = addr;
	cmd->blk_size = blk_size;
	cmd->blk_num = 1;
	cmd->buf = dst;

	err = mmc_send_cmd(mmc_dev, cmd);
	if (err) {
		SYS_LOG_ERR("sigle_blk r/w failed, ret %d", err);
		return err;
	}

	return 0;
}

int mmc_multi_blk_rw(struct device *dev, int is_write, unsigned int addr,
		     void *dst, int blk_size, int blk_num)
{
	struct sd_card_data *sd = dev->driver_data;
	struct device *mmc_dev = sd->mmc_dev;
	struct mmc_cmd *cmd = &sd->cmd;
	int err;
	struct mmc_csd *csd = &sd->mmc_csd;

	/* if support cmd23, just need to send cmd23 and then cmd25/cmd18 */
	if (csd->suport_cmd23 && !sd->on_high_performance) {
		err = mmc_set_blockcount(mmc_dev, cmd, blk_num, 0);
		if (err) {
			SYS_LOG_ERR("mmc_set_blockcount r/w failed, ret %d", err);
			return err;
		}
	}

	memset(cmd, 0, sizeof(struct mmc_cmd));

	if (is_write) {
		cmd->opcode = MMC_WRITE_MULTIPLE_BLOCK;
		cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC | MMC_DATA_WRITE;
	} else {
		cmd->opcode = MMC_READ_MULTIPLE_BLOCK;
		cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC | MMC_DATA_READ;
	}

	/* record the next read/wirte offset and at next transmission if hit the read/write offset will not send stop command  */
	if (sd->on_high_performance) {
		SYS_LOG_DBG("%d next_rw_offs 0x%x, prev_is_write %d, is_write %d, addr %d",
			__LINE__, sd->next_rw_offs, sd->prev_is_write, is_write, addr);
		if (sd->next_rw_offs == SD_CARD_INVALID_OFFSET) {
			sd->next_rw_offs = (addr + blk_num);
		} else {
			if ((sd->prev_is_write) && (is_write) && (addr == sd->next_rw_offs)) {
				cmd->flags = (MMC_RSP_R1 | MMC_CMD_ADTC | MMC_DATA_WRITE_DIRECT);
				sd->next_rw_offs += blk_num;
				SYS_LOG_DBG("%d, next_rw_offs 0x%x, blk_num %d", __LINE__, sd->next_rw_offs, blk_num);
			} else if ((!sd->prev_is_write) && (!is_write) && (addr == sd->next_rw_offs)){
				cmd->flags = (MMC_RSP_R1 | MMC_CMD_ADTC | MMC_DATA_READ_DIRECT);
				sd->next_rw_offs += blk_num;
				SYS_LOG_DBG("%d, next_rw_offs 0x%x, blk_num %d", __LINE__, sd->next_rw_offs, blk_num);
			} else {
				SYS_LOG_DBG("%d STOP", __LINE__);
				struct mmc_cmd stop_cmd = {0};
				err = mmc_stop_block_transmission(mmc_dev, &stop_cmd);
				if (err) {
					SYS_LOG_ERR("mmc stop block transmission failed, ret %d", err);
					return err;
				}
				sd->next_rw_offs = addr + blk_num;
			}
		}

		if (is_write)
			sd->prev_is_write = true;
		else
			sd->prev_is_write = false;
	}

	cmd->arg = addr;
	cmd->blk_size = blk_size;
	cmd->blk_num = blk_num;
	cmd->buf = dst;

	err = mmc_send_cmd(mmc_dev, cmd);
	if (err) {
		SYS_LOG_ERR("multi_blk r/w failed, ret %d", err);
		return err;
	}

	/* if not support cmd23, need to send the cmd12 to stop the transmission */
	if (!csd->suport_cmd23 && !sd->on_high_performance) {
		err = mmc_stop_block_transmission(mmc_dev, cmd);
		if (err) {
			SYS_LOG_ERR("mmc stop block transmission failed, ret %d", err);
			return err;
		}
	}

	return 0;
}

static int mmc_card_busy_detect(struct device *dev, u32_t timeout_ms)
{
	struct sd_card_data *sd = dev->driver_data;
	u32_t status = 0;
	u32_t start_time, curr_time;

	start_time = k_uptime_get();

	get_card_status(sd, &status);

	while (!(status & R1_READY_FOR_DATA) ||
		R1_CURRENT_STATE(status) == R1_STATE_PRG) {
		SYS_LOG_DBG("card busy, status 0x%x", status);
		get_card_status(sd, &status);

		curr_time = k_uptime_get_32();
		if ((curr_time - start_time) >= timeout_ms) {
			SYS_LOG_ERR("wait card busy timeout, status 0x%x", status);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

int sd_card_rw(struct device *dev, int rw, u32_t sector_offset,
	       void *data, size_t sector_cnt)
{
	struct sd_card_data *sd = dev->driver_data;
	struct mmc_csd *csd = &sd->mmc_csd;
	u32_t addr, chunk_sector_cnt, chunk_size;
	int err = 0;
	u8_t i;

#ifdef CONFIG_MMC_SDCARD_SHOW_PERF
	u32_t start_time, end_time, delta, cur_xfer_size;
	start_time = k_cycle_get_32();
	cur_xfer_size = sector_cnt * SD_CARD_SECTOR_SIZE;
#endif

	SYS_LOG_DBG("rw%d, sector_offset 0x%x, data %p, sector_cnt %zu\n",
		    rw, sector_offset, data, sector_cnt);

	if ((sector_offset + sector_cnt) > csd->sector_count) {
		return -EINVAL;
	}

	k_sem_take(&sd->lock, K_FOREVER);

	if (!sd->card_initialized) {
		k_sem_give(&sd->lock);
		return -ENOENT;
	}

#ifdef CONFIG_MMC_SDCARD_LOW_POWER
	mmc_exit_low_power(sd);
#endif

	addr = csd->ccs ? sector_offset :
			  sector_offset * SD_CARD_SECTOR_SIZE;

	/* The number of blocks at multi block transmision is 65536 in sdmmc protocol */
	if (sd->on_high_performance)
		chunk_sector_cnt = SD_CARD_RW_MAX_SECTOR_CNT_PROTOCOL;
	else
		chunk_sector_cnt = SD_CARD_RW_MAX_SECTOR_CNT;

	while (sector_cnt > 0) {
		if (sector_cnt < chunk_sector_cnt) {
			chunk_sector_cnt = sector_cnt;
		}

		chunk_size = chunk_sector_cnt * SD_CARD_SECTOR_SIZE;

		if (chunk_sector_cnt > 1) {
			for (i = 0; i < (CONFIG_MMC_SDCARD_ERR_RETRY_NUM + 1); i++) {
				err = mmc_multi_blk_rw(dev, rw, addr, data,
					SD_CARD_SECTOR_SIZE, chunk_sector_cnt);
					if (!err)
						break;
			}
		} else {
			for (i = 0; i < (CONFIG_MMC_SDCARD_ERR_RETRY_NUM + 1); i++) {
				err = mmc_sigle_blk_rw(dev, rw, addr, data,
					SD_CARD_SECTOR_SIZE);
					if (!err)
						break;
			}
		}
		/* we need to wait the card program status when write mode */
		if (rw)
			err |= mmc_card_busy_detect(dev, SD_CARD_WAITBUSY_TIMEOUT_MS);

		if (err) {
			SYS_LOG_ERR("Failed: rw:%d, addr:0x%x, scnt:0x%x!",
				rw, addr, chunk_sector_cnt);

			/* card error, need reinitialize */
			sd->force_plug_out = true;
			sd->card_initialized = false;
			err = -EIO;
			break;
		}

		if (csd->ccs)
			addr += chunk_sector_cnt;
		else
			addr += chunk_size;

		data += chunk_size;
		sector_cnt -= chunk_sector_cnt;
	}

#ifdef CONFIG_MMC_SDCARD_LOW_POWER
	mmc_enter_low_power(sd);
#endif

	k_sem_give(&sd->lock);

#ifdef CONFIG_MMC_SDCARD_SHOW_PERF
	end_time = k_cycle_get_32();
	delta = SYS_CLOCK_HW_CYCLES_TO_NS(end_time - start_time);
	delta /= 1000UL;
	if (rw) {
		sd->rec_perf_wr_size += cur_xfer_size;
		if (delta > sd->max_wr_use_time)
			sd->max_wr_use_time = delta;
	} else {
		sd->rec_perf_rd_size += cur_xfer_size;
		if (delta > sd->max_rd_use_time)
			sd->max_rd_use_time = delta;
	}

	if (SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32()
			- sd->rec_perf_timestamp) > 1000000000UL) {
		SYS_LOG_INF("bandwidth read %dB/s write %dB/s",
					sd->rec_perf_rd_size, sd->rec_perf_wr_size);
		SYS_LOG_INF("current rw:%d speed %dB/s",
					rw, cur_xfer_size * 1000000 / delta);
		SYS_LOG_INF("max read time %dus, max write time %dus",
					sd->max_rd_use_time, sd->max_wr_use_time);
		sd->rec_perf_timestamp = k_cycle_get_32();
		sd->rec_perf_rd_size = 0;
		sd->rec_perf_wr_size = 0;
	}
#endif

	return err;
}

static int sd_scan_host(struct device *dev)
{
	struct sd_card_data *sd = dev->driver_data;
	struct device *mmc_dev = sd->mmc_dev;
	int err;
	u32_t ocr, rocr;
	u32_t cid_csd[4];
	u32_t scr[16] = {0};
	u32_t caps;

	ocr = 0x40ff8000;

	k_sem_take(&sd->lock, K_FOREVER);

	/* init controller default clock and width */
	mmc_set_clock(mmc_dev, SD_CARD_INIT_CLOCK_FREQ);
	mmc_set_bus_width(mmc_dev, MMC_BUS_WIDTH_1);

	mmc_go_idle(mmc_dev, &sd->cmd);
	mmc_send_if_cond(mmc_dev, &sd->cmd, ocr);

	/* try CARD_TYPE_SD first */
	sd->card_type = CARD_TYPE_SD;
	err = mmc_send_app_op_cond(mmc_dev, &sd->cmd, ocr, &rocr);
	if (err) {
		/* try eMMC card */
		err = emmc_send_app_op_cond(mmc_dev, &sd->cmd, ocr, &rocr);
		if (err)
			goto out;
		sd->card_type = CARD_TYPE_MMC;
	}

	sd->mmc_csd.ccs = (rocr & SD_OCR_CCS)? 1 : 0;

	err = mmc_all_send_cid(mmc_dev, &sd->cmd, cid_csd);
	if (err)
		goto out;

	if (sd->card_type == CARD_TYPE_MMC) {
		sd->rca = 1;
		sd->mmc_csd.suport_cmd23 = 1;

		err = emmc_send_relative_addr(mmc_dev, &sd->cmd, &sd->rca);
		if (err)
			goto out;

		err = mmc_send_csd(mmc_dev, &sd->cmd, sd->rca, cid_csd);
		if (err)
			goto out;

		err = emmc_decode_csd(sd, cid_csd);
		if (err)
			goto out;

		err = mmc_select_card(mmc_dev, &sd->cmd, sd->rca);
		if (err)
			goto out;
	} else {
		err = mmc_send_relative_addr(mmc_dev, &sd->cmd, &sd->rca);
		if (err)
			goto out;

		err = mmc_send_csd(mmc_dev, &sd->cmd, sd->rca, cid_csd);
		if (err)
			goto out;

		err = mmc_decode_csd(sd, cid_csd);
		if (err)
			goto out;

		err = mmc_select_card(mmc_dev, &sd->cmd, sd->rca);
		if (err)
			goto out;

		err = mmc_app_send_scr(mmc_dev, &sd->cmd, sd->rca, scr);
		if (err)
			goto out;

		mmc_decode_scr(sd, scr);
	}

	/* set bus speed */
	if (CARD_TYPE_SD == sd->card_type)
		err = mmc_sd_switch_hs(mmc_dev);
	else
		err = emmc_switch_hs(mmc_dev);
	if (err > 0) {
		mmc_set_clock(mmc_dev, SD_CARD_SDR25_CLOCK_FREQ);
	} else {
		mmc_set_clock(mmc_dev, SD_CARD_SDR12_CLOCK_FREQ);
	}

	/* set bus width */
	caps = mmc_get_capability(mmc_dev);
	if (caps & MMC_CAP_4_BIT_DATA) {
		if (CARD_TYPE_SD == sd->card_type) {
			err = mmc_app_set_bus_width(mmc_dev, &sd->cmd, sd->rca, MMC_BUS_WIDTH_4);
			if (err)
				goto out;
		} else {
			err = emmc_switch(mmc_dev, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_WIDTH_4);
			if (err)
				goto out;
		}

		mmc_set_bus_width(mmc_dev, MMC_BUS_WIDTH_4);
	}

	/* FIXME: workaround to put ext_csd reading here, since fail just after reading csd at present */
	if (sd->card_type == CARD_TYPE_MMC && sd->ext_csd.mmca_vsn >= CSD_SPEC_VER_4) {
		/*
		 * As the ext_csd is so large and mostly unused, we don't store the
		 * raw block in mmc_card.
		 */
		u8_t *ext_csd = mem_malloc(512);

		if (!ext_csd) {
			err = -ENOMEM;
			goto out;
		}

		err = mmc_send_ext_csd(mmc_dev, &sd->cmd, ext_csd);
		if (err) {
			mem_free(ext_csd);
			goto out;
		}

		err = emmc_read_ext_csd(sd, ext_csd);
		if (err) {
			mem_free(ext_csd);
			goto out;
		}
		mem_free(ext_csd);
	}

	if (sd->mmc_csd.sector_size != SD_CARD_SECTOR_SIZE) {
		sd->mmc_csd.sector_count = sd->mmc_csd.sector_count *
			(sd->mmc_csd.sector_size / SD_CARD_SECTOR_SIZE);
		sd->mmc_csd.sector_size = SD_CARD_SECTOR_SIZE;
	}

	err = 0;
	sd->card_initialized = true;
	sd->next_rw_offs = SD_CARD_INVALID_OFFSET;
	sd->on_high_performance = false;

#ifdef CONFIG_MMC_SDCARD_SHOW_PERF
	sd->max_rd_use_time = 0;
	sd->max_wr_use_time = 0;
	sd->rec_perf_rd_size = 0;
	sd->rec_perf_wr_size = 0;
	sd->rec_perf_timestamp = 0;
#endif

	SYS_LOG_INF("sdcard is plugged");

out:
	k_sem_give(&sd->lock);

	return err;
}

#ifdef BOARD_SDCARD_DETECT_GPIO
static int sd_card_check_card_by_gpio(struct sd_card_data *sd)
{
	int err, value;
	/* card detect task already has the debounce operation*/
	err = gpio_pin_read(sd->detect_gpio_dev, BOARD_SDCARD_DETECT_GPIO,
			    &value);
	if (err) {
		return -EIO;
	}

	if (value) {
		/* card is not detected */
		return false;
	}

	/* card is detected */
	return true;
}

/*
* return
* 0 disk ok
* 1 STA_NOINIT
* 2 STA_NODISK
* other: unknow error
*/
static int sd_card_detect(struct device *dev)
{
	struct sd_card_data *sd = dev->driver_data;

	if (sd->force_plug_out) {
		SYS_LOG_INF("sdcard plug out forcely due to rw error");
		sd->force_plug_out = false;
		sd->card_initialized = false;
		return STA_NODISK;
	}
#if 0
	if (sd->card_initialized) {
		if (!sd_card_is_unplugged(sd)) {
			return STA_DISK_OK;
		}

		SYS_LOG_INF("sdcard is unplugged");

		k_sem_take(&sd->lock, K_FOREVER);
		sd->card_initialized = false;
		k_sem_give(&sd->lock);
	} else {
		if (sd_card_check_card_by_gpio(sd) == true) {
			return STA_NOINIT;
		}
	}
#else
	if (sd_card_check_card_by_gpio(sd) == true) {
		if (sd->card_initialized)
			return STA_DISK_OK;
		else
			return STA_NOINIT;
	} else {
		mmc_release_device(sd->mmc_dev);
		sd->card_initialized = false;
	}
#endif
	return STA_NODISK;
}
#else
static int sd_card_is_unplugged(struct sd_card_data *sd)
{
	const int retry_times = 3;
	int err, i;
	u32_t status;

	/* assume emmc is always exist */
	if (sd->card_type == CARD_TYPE_MMC) {
		return false;
	}

	k_sem_take(&sd->lock, K_FOREVER);

	for (i = 0; i < retry_times; i++) {
		err = get_card_status(sd, &status);
		status = R1_CURRENT_STATE(status);
		if (!err && (status == R1_STATE_TRAN ||
			(status == R1_STATE_STBY) ||
			(status == R1_STATE_RCV) ||
			(status == R1_STATE_DATA))) {
			break;
		}
	}

	k_sem_give(&sd->lock);

	if (i == retry_times) {
		return true;
	}

	return false;
}

static int sd_card_detect(struct device *dev)
{
	struct sd_card_data *sd = dev->driver_data;
	int err, ret = STA_NODISK;

	if (sd->force_plug_out) {
		SYS_LOG_INF("sdcard plug out forcely due to rw error");
		sd->force_plug_out = false;
		sd->card_initialized = false;
		return STA_NODISK;
	}

	/* check card status */
	if (!sd->card_initialized) {
		/* detect card by send init commands */
		err = sd_card_storage_init(dev);
		if (!err) {
			ret = STA_DISK_OK;
		}
	} else {
		if (sd_card_is_unplugged(sd)) {
			SYS_LOG_INF("sdcard is unplugged");
			sd->card_initialized = false;
		} else {
			ret = STA_DISK_OK;
		}
	}

	return ret;
}
#endif

#if 0
int sd_card_scan_delay_chain(struct device *dev)
{
	struct sd_card_data *sd = dev->driver_data;
	int err;
	u32_t status;
	u8_t rd;

	printk("%s: scan sd card delay chain\n", __func__);

	for (rd = 0; rd < 0x0f; rd++) {
		printk("%s: scan read delay chain: %d\n", __func__, rd);

		sd_card_set_delay_chain(dev, rd, 0xff);

		err = get_card_status(sd, &status);
		status = R1_CURRENT_STATE(status);
		if (err || (status != R1_STATE_TRAN &&
			status != R1_STATE_STBY &&
			status != R1_STATE_RCV)) {
			continue;
		}

		err = sd_card_storage_read(dev, 0, tmp_card_buf, 1);
		if (err) {
			continue;
		}

		printk("%s: scan read delay chain: %d pass\n", __func__, rd);
	}

	return 0;
}
#endif
extern int board_mmc_power_reset(int mmc_id, u8_t cnt);

static int sd_card_power_reset(struct sd_card_data *sd, u8_t cnt)
{
	return board_mmc_power_reset(0, cnt);
}

static int sd_card_storage_init(struct device *dev)
{
	struct sd_card_data *sd = dev->driver_data;
	int ret;
	u8_t i, cnt;

	if (!sd->mmc_dev)
		return -ENODEV;

#ifdef BOARD_SDCARD_DETECT_GPIO
	cnt = CONFIG_MMC_SDCARD_RETRY_TIMES;
#else
	/* In case of without detect pin will depend on the hotplug thread to initialize */
	cnt = 1;
#endif

	k_sem_take(&sd->lock, K_FOREVER);
	if (sd->card_initialized) {
		SYS_LOG_DBG("SD card has already initialized!");
		k_sem_give(&sd->lock);
		return 0;
	}
	k_sem_give(&sd->lock);

	for (i = 0; i < cnt; i++) {
		sd_card_power_reset(sd, i + 1);
		ret = sd_scan_host(dev);
		if (!ret)
			break;
		else
			SYS_LOG_DBG("SD card storage init failed");

#ifdef BOARD_SDCARD_DETECT_GPIO
		if (sd_card_check_card_by_gpio(sd) == false) {
			k_sleep(100);
			if (sd_card_check_card_by_gpio(sd) == false) {
				SYS_LOG_ERR("SD card is not detected");
				break;
			}
		}
#endif
	}

	if (!ret) {
		SYS_LOG_INF("SD card storage initialized!\n");
	} else {
		ret = -ENODEV;
	}
	return ret;
}

static int sd_card_storage_read(struct device *dev, off_t offset, void *data,
			   size_t len)
{
	return sd_card_rw(dev, 0, offset, data, len);
}

static int sd_card_storage_write(struct device *dev, off_t offset,
				const void *data, size_t len)
{
	return sd_card_rw(dev, 1, offset, (void *)data, len);
}

static int sd_card_enter_high_speed(struct device *dev)
{
#ifdef BOARD_SDCARD_DETECT_GPIO
	struct sd_card_data *sd = dev->driver_data;
	k_sem_take(&sd->lock, K_FOREVER);
	if (sd->on_high_performance) {
		SYS_LOG_DBG("already enter high speed mode");
		goto out;
	}

	sd->on_high_performance = true;
out:
	k_sem_give(&sd->lock);

	SYS_LOG_INF("enter high speed mode");
#endif
	return 0;
}

static int sd_card_exit_high_speed(struct device *dev)
{
#ifdef BOARD_SDCARD_DETECT_GPIO
	int ret;
	u32_t status;
	struct sd_card_data *sd = dev->driver_data;
	k_sem_take(&sd->lock, K_FOREVER);
	if (!sd->on_high_performance) {
		SYS_LOG_DBG("already exit high speed mode");
		ret = 0;
		goto out;
	}

	ret = mmc_send_status(sd->mmc_dev, &sd->cmd, sd->rca, &status);
	if (!ret) {
		status = R1_CURRENT_STATE(status);
		if ((status == R1_STATE_DATA) || (status == R1_STATE_RCV)) {
			SYS_LOG_INF("status:0x%x send stop command", status);
			mmc_stop_block_transmission(sd->mmc_dev, &sd->cmd);
		}
	}
	sd->on_high_performance = false;
	sd->next_rw_offs = SD_CARD_INVALID_OFFSET;
	SYS_LOG_INF("sd->next_rw_offs %d", sd->next_rw_offs);

out:
	k_sem_give(&sd->lock);

	SYS_LOG_INF("exit high speed mode");

	return ret;
#else
	return 0;
#endif
}

static int sd_card_storage_ioctl(struct device *dev, u8_t cmd, void *buff)
{
	struct sd_card_data *sd = dev->driver_data;
	int ret;

	if (!sd->card_initialized) {
		if ((cmd != DISK_IOCTL_HW_DETECT)
			&& (cmd != DISK_IOCTL_HOTPLUG_PERIOD_DETECT)) {
			SYS_LOG_DBG("error command %d", cmd);
			return -ENOENT;
		}
	}

	switch (cmd) {
	case DISK_IOCTL_CTRL_SYNC:
		break;
	case DISK_IOCTL_GET_SECTOR_COUNT:
		*(u32_t *)buff = sd->mmc_csd.sector_count;
		break;
	case DISK_IOCTL_GET_SECTOR_SIZE:
		*(u32_t *)buff = sd->mmc_csd.sector_size;
		break;
	case DISK_IOCTL_GET_ERASE_BLOCK_SZ:
		if (sd->card_type == CARD_TYPE_SD) {
			*(u32_t *)buff  = SD_CARD_SECTOR_SIZE;
		} else {
			*(u32_t *)buff  = sd->ext_csd.erase_size / sd->mmc_csd.sector_size;
		}
		break;
	case DISK_IOCTL_GET_DISK_SIZE:
		/* > 4GByte??, add 2GByte replase */
		*(u32_t *)buff  = (sd->mmc_csd.ccs)? 0x80000000 :
			(sd->mmc_csd.sector_size * sd->mmc_csd.sector_count);
		break;
	case DISK_IOCTL_HW_DETECT:
		if (sd->card_initialized)
			*(u8_t *)buff = STA_DISK_OK;
		else
			*(u8_t *)buff = STA_NODISK;
		break;
	case DISK_IOCTL_HOTPLUG_PERIOD_DETECT:
		ret = sd_card_detect(dev);
		if (STA_NOINIT == ret || STA_DISK_OK == ret) {
			*(u8_t *)buff = STA_DISK_OK;
		} else {
			*(u8_t *)buff = STA_NODISK;
		}
		break;
	case DISK_IOCTL_ENTER_HIGH_SPEED:
		ret = sd_card_enter_high_speed(dev);
		if (ret)
			return ret;
		break;
	case DISK_IOCTL_EXIT_HIGH_SPEED:
		ret = sd_card_exit_high_speed(dev);
		if (ret)
			return ret;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct flash_driver_api sd_card_storage_api = {
	.init = sd_card_storage_init,
	.read = sd_card_storage_read,
	.write = sd_card_storage_write,
	.ioctl = sd_card_storage_ioctl,
};

static int sd_card_init(struct device *dev)
{
	struct sd_card_data *sd = dev->driver_data;
	int ret = 0;

	sd->mmc_dev = device_get_binding(CONFIG_MMC_SDCARD_MMC_DEV_NAME);
	if (!sd->mmc_dev) {
		SYS_LOG_ERR("Cannot find mmc device %s!\n",
			    CONFIG_MMC_SDCARD_MMC_DEV_NAME);
		return -ENODEV;
	}

#ifdef BOARD_SDCARD_DETECT_GPIO
	sd->detect_gpio_dev = device_get_binding(BOARD_SDCARD_DETECT_GPIO_NAME);
	if (!sd->detect_gpio_dev) {
		SYS_LOG_ERR("Sd requires GPIO. Cannot find %s!\n",
				BOARD_SDCARD_DETECT_GPIO_NAME);
		return -ENODEV;
	}

	/* switch gpio function to input for detecting card plugin */
	ret = gpio_pin_configure(sd->detect_gpio_dev,
				 BOARD_SDCARD_DETECT_GPIO, GPIO_DIR_IN);
	if (ret) {
		return ret;
	}
#endif

	k_sem_init(&sd->lock, 1, 1);
	sd->is_low_power = false;
	sd->card_initialized = false;
	sd->force_plug_out = false;
	sd->card_type = CARD_TYPE_SD;

	return ret;
}


#ifdef CONFIG_MMC_SDCARD
struct sd_card_data sdcard_acts_data_0;

DEVICE_AND_API_INIT(sd_storage, CONFIG_MMC_SDCARD_DEV_NAME, sd_card_init,
		&sdcard_acts_data_0, NULL,
		POST_KERNEL, 35,
		&sd_card_storage_api);
#endif
