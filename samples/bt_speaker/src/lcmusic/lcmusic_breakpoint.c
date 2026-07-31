/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief lcmusic app breakpoint.
 */

#include "lcmusic.h"
#include <property_manager.h>

void _lcmusic_bpinfo_dump(struct music_bp_info_t *bp)
{
	if(NULL == bp) {
		SYS_LOG_ERR("bp is NULL");
		return;
	}

	SYS_LOG_INF("%d/%d/%d", bp->track_no,
		bp->file_dir_info[0].cluster,
		bp->file_dir_info[0].dirent_blk_ofs);

	SYS_LOG_INF("time:%d ms, file:%d/%d bytes\n",
		    bp->bp_info.time_offset,
		    bp->bp_info.file_offset,
		    bp->file_size);
}

void _lcmusic_bpinfo_save(const char *dir, struct music_bp_info_t *bp)
{
	int ret = 0;
	char *tags[3] = { "SDCAR_BP_INFO", "USB_BP_INFO", "NOR_BP_INFO" };
	u8_t tag = 0;

	SYS_LOG_INF("");

	_lcmusic_bpinfo_dump(bp);

	if (strstr(dir, "SD:")) {
		tag = 0;
	} else if (strstr(dir, "USB:")) {
		tag = 1;
	} else {
		tag = 2;
	}

#ifdef CONFIG_PROPERTY
	ret = property_set(tags[tag], (char *)bp, sizeof(struct music_bp_info_t));
	if (ret < 0) {
		SYS_LOG_ERR("failed %d", ret);
	}
#endif
}

int _lcmusic_bpinfo_resume(const char *dir, struct music_bp_info_t *bp)
{
	int ret = 0;
	char *tags[3] = { "SDCAR_BP_INFO", "USB_BP_INFO", "NOR_BP_INFO" };
	u8_t tag = 0;

	SYS_LOG_INF("");

	if (strstr(dir, "SD:")) {
		tag = 0;
	} else if (strstr(dir, "USB:")) {
		tag = 1;
	} else {
		tag = 2;
	}

#ifdef CONFIG_PROPERTY
	ret = property_get(tags[tag], (char *)bp,
			 sizeof(struct music_bp_info_t));
	if (ret < 0) {
		SYS_LOG_ERR("failed %d", ret);
	}
#endif
	_lcmusic_bpinfo_dump(bp);

	return ret;
}
