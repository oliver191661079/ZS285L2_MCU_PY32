/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SYS_LOG_LEVEL 3
#define SYS_LOG_DOMAIN "otalib"
#include <logging/sys_log.h>

#include <kernel.h>
#include <string.h>
#include <nvram_config.h>
#include <partition.h>
#include <ota_backend.h>
#include "ota_manifest.h"
#include "ota_breakpoint.h"
#include <os_common_api.h>

static struct ota_breakpoint ota_bp;


void ota_breakpoint_dump(struct ota_breakpoint *bp)
{
#if CONFIG_SYS_LOG_DEFAULT_LEVEL > 2
	struct ota_file *cur_file = &bp->cur_file;
	int i;

	printk("  breakpoint: %p\n", bp);
	printk("  old_version 0x%x, new_version 0x%x, data_checksum 0x%x\n",
		bp->old_version, bp->new_version, bp->data_checksum);
	printk("  bp_id %d, state %d, backend_type %d\n",
		bp->bp_id, bp->state, bp->backend_type);

	for (i = 0; i < ARRAY_SIZE(bp->file_state); i++) {
		if (bp->file_state[i].file_id > 0)
		printk("  [%d] file_id %d state 0x%x\n", i,
			bp->file_state[i].file_id, bp->file_state[i].state);
	}

	printk("  cur file %s: file_id %d, offset 0x%x, size 0x%x\n",
		cur_file->name, cur_file->file_id, cur_file->offset, cur_file->size);
	printk("  write_offset 0x%x\n", bp->cur_file_write_offset);

#endif

}

int ota_breakpoint_check_bp_need_update(struct ota_breakpoint *bp)
{
	if(memcmp(&ota_bp, bp, sizeof(struct ota_breakpoint)) != 0){
		return true;
	}else{
		return false;
	}
}

int ota_breakpoint_save(struct ota_breakpoint *bp)
{
	SYS_LOG_INF("bp_state %d", bp->state);

	if(ota_breakpoint_check_bp_need_update(bp)){
		/* update seq id */
		bp->bp_id++;
	}else{
		return 0;
	}

	ota_breakpoint_dump(bp);

	memcpy(&ota_bp, bp, sizeof(struct ota_breakpoint));

#ifdef CONFIG_OTA_ENABLE_REBOOT_RESUME_UPGRADE
	nvram_config_set("OTA_BP", bp, sizeof(struct ota_breakpoint));
#endif

	return 0;
}

int ota_breakpoint_file_state(struct ota_breakpoint *bp)
{
	return ota_breakpoint_save(bp);
}

int ota_breakpoint_load(struct ota_breakpoint *bp)
{
#ifdef CONFIG_OTA_ENABLE_REBOOT_RESUME_UPGRADE
	int rlen;
	memset(&ota_bp, 0, sizeof(struct ota_breakpoint));
	rlen = nvram_config_get("OTA_BP", &ota_bp, sizeof(struct ota_breakpoint));
	if (rlen <= 0 || rlen > sizeof(struct ota_breakpoint)) {
		return -1;
	}
#endif

	memcpy(bp, &ota_bp, sizeof(struct ota_breakpoint));

	if(bp->state > OTA_BP_STATE_UPGRADING_FAIL){
		return -1;
	}

	if(bp->mirror_id > 2){
		return -1;
	}

	SYS_LOG_INF("bp_state %d", bp->state);
	ota_breakpoint_dump(bp);

	return 0;
}

int ota_breakpoint_get_current_state(struct ota_breakpoint *bp)
{
	return bp->state;
}

int ota_breakpoint_set_file_state(struct ota_breakpoint *bp, int file_id, int state)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bp->file_state); i++) {
		if (bp->file_state[i].file_id == file_id) {
			//SYS_LOG_INF("set file_id %d file state 0x%x\n", file_id, state);
			bp->file_state[i].state = state;
			return 0;
		}
	}

	/* not found, allocate new slot for this file */
	for (i = 0; i < ARRAY_SIZE(bp->file_state); i++) {
		if (bp->file_state[i].file_id == 0) {
			//SYS_LOG_INF("set file_id %d file state 0x%x\n", file_id, state);
			bp->file_state[i].file_id = file_id;
			bp->file_state[i].state = state;
			return 0;
		}
	}

	/* no slot? */
	SYS_LOG_INF("file_id %d fail", file_id);
	return -1;
}

int ota_breakpoint_get_file_state(struct ota_breakpoint *bp, int file_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bp->file_state); i++) {
		if (bp->file_state[i].file_id == file_id) {
			return bp->file_state[i].state;
		}
	}

	/* no slot? */
	SYS_LOG_INF("file_id %d not found", file_id);
	return -1;
}

int ota_breakpoint_clear_all_file_state(struct ota_breakpoint *bp)
{
	int i;

	SYS_LOG_INF();

	for (i = 0; i < ARRAY_SIZE(bp->file_state); i++) {
		bp->file_state[i].file_id = 0;
		bp->file_state[i].state = 0;
	}

	memset(&bp->cur_file, 0x0, sizeof(struct ota_file));
	bp->cur_file_write_offset = 0;
	bp->data_checksum = 0;

	return 0;
}

int ota_breakpoint_update_state(struct ota_breakpoint *bp, int state)
{
	SYS_LOG_INF("bp_state %d", state);

	bp->state = state;

	if (state == OTA_BP_STATE_UPGRADE_DONE ||
	    state == OTA_BP_STATE_UPGRADE_PENDING) {
		ota_breakpoint_clear_all_file_state(bp);
	}

	ota_breakpoint_save(bp);

	return 0;
}

int ota_breakpoint_update_file_state(struct ota_breakpoint *bp, struct ota_file *file,
		int state, int cur_offset, int force)
{
	int need_save;

	SYS_LOG_DBG("file_id=%d state=%d offs=0x%x", file->file_id, state, cur_offset);

	need_save = 0;

	/* aligned with erase sector */
	switch (state) {
	case OTA_BP_FILE_STATE_WRITE_START:
		memcpy(&bp->cur_file, file, sizeof(struct ota_file));
		bp->cur_file_write_offset = cur_offset;
		break;

	case OTA_BP_FILE_STATE_WRITING:
		if (force || (cur_offset - bp->cur_file_write_offset) == 0 ||
		    (cur_offset - bp->cur_file_write_offset) >= 128 * 1024) {
			need_save = 1;
			bp->cur_file_write_offset = cur_offset;
		}

		break;

	default:
		need_save = 1;
		break;
	}

	ota_breakpoint_set_file_state(bp, file->file_id, state);

	if (need_save){
		ota_breakpoint_save(bp);
	}

	return need_save;
}

int ota_breakpoint_init_default_value(struct ota_breakpoint *bp)
{
	memset(bp, 0x0, sizeof(struct ota_breakpoint));

	bp->old_version = fw_version_get_code();
	bp->mirror_id = !partition_get_current_mirror_id();
	bp->state = OTA_BP_STATE_UNKOWN;

	return 0;
}

int ota_breakpoint_init(struct ota_breakpoint *bp)
{
	int err;

	SYS_LOG_INF("init bp %p\n", bp);

	err = ota_breakpoint_load(bp);
	if (err) {
		SYS_LOG_INF("no bp in nvram, use default");
		ota_breakpoint_init_default_value(bp);
		ota_breakpoint_save(bp);
	}

	return 0;
}

void ota_breakpoint_exit(struct ota_breakpoint *bp)
{
	SYS_LOG_INF("deinit bp %p\n", bp);

	ota_breakpoint_save(bp);
}

int ota_breakpoint_reset(struct ota_breakpoint *bp)
{
	ota_breakpoint_init_default_value(bp);
	ota_breakpoint_save(bp);

	return 0;
}

