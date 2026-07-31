/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA upgrade interface
 */

#define SYS_LOG_LEVEL 3
#define SYS_LOG_DOMAIN "otalib"
#include <logging/sys_log.h>

#include <kernel.h>
#include <string.h>
#include <device.h>
#include <flash.h>
#include <soc.h>
#include <fw_version.h>
#include <partition.h>
#include <mem_manager.h>
#include <crc.h>
#include <ota_upgrade.h>
#include <ota_backend.h>
#include "ota_image.h"
#include "ota_storage.h"
#include "ota_manifest.h"
#include "ota_breakpoint.h"
#include "ota_file_patch.h"
#include <os_common_api.h>
#include <logging/sys_log.h>
#include <acts_ringbuf.h>
#include <ota_dfu_breakpoint.h>

#define OTA_ERASE_ALIGN_SIZE		4096
#define OTA_DATA_BUFFER_SIZE		1024

#define OTA_MANIFESET_FILE_NAME		"ota.xml"

#define OTA_FLAG_USE_RECOVERY		(1 << 0)
#define OTA_FLAG_USE_RECOVERY_APP	(1 << 1)
#define OTA_FLAG_USE_NO_VERSION_CONTROL	(1 << 2)
#define OTA_FLAG_USE_SECURE_BOOT    (1 << 3)

#define ota_use_recovery(ota)		((ota)->flags & OTA_FLAG_USE_RECOVERY)
#define ota_use_recovery_app(ota)	((ota)->flags & OTA_FLAG_USE_RECOVERY_APP)
#define ota_use_no_version_control(ota)	((ota)->flags & OTA_FLAG_USE_NO_VERSION_CONTROL)
#define ota_use_secure_boot(ota)	((ota)->flags & OTA_FLAG_USE_SECURE_BOOT)

#define EIO_READ			(1001)

#define OTA_BPREPORT (1)  // Indicate that ota will resume breakpoint
#define OTA_FWVERSION_LIMIT_100 (1)  // Max to 100 every version byte

#ifdef CONFIG_OTA_LEGACY_REQ_SIZE
#define OTA_REQ_MAX_SIZE			(4*1024)
#else
#define OTA_REQ_MAX_SIZE			(128*1024)
#endif

#define OTA_RX_STACKSIZE			(2048)
#ifdef CONFIG_OTA_BACKEND_BLUETOOTH
#define OTA_RX_BUFSIZE				(78*1024)
#define OTA_IN_BUFSIZE				(40*1024)
#else
#define OTA_RX_BUFSIZE				(16*1024)
#define OTA_IN_BUFSIZE				(16*1024)
#endif

#define OTA_SIGNATURE_DATA_LEN		(256)

static __ota_bss uint32_t ota_rx_stack[OTA_RX_STACKSIZE / 4] ;
static __ota_bss uint8_t ota_rx_data_buf[OTA_RX_BUFSIZE];
static __ota_bss uint8_t ota_rx_in_buf[OTA_IN_BUFSIZE];

struct ota_rx_info {
	char *rx_stack;
	uint8_t *rx_buf;
	uint32_t rx_bufsize;
	uint8_t *in_buf;
	uint32_t in_bufsize;

	os_sem rx_get_sem;
	os_sem rx_put_sem;
	struct acts_ringbuf rbuf;
	os_tid_t rx_tid;

	uint32_t offset;
	uint32_t size;
	uint32_t seg_size;
	uint32_t file_crc;
	int rx_errno;
	uint8_t is_raw;
	uint8_t thread_terminated;
	uint8_t thread_need_terminated;
	uint8_t file_id;
	struct k_thread rx_info_thread;
};

struct ota_upgrade_info {
	int state;
	unsigned int flags;

	ota_notify_t notify;

	struct ota_image *img;
	struct ota_storage *storage;
#ifdef CONFIG_OTA_DOWNLOAD_EXTERNAL_FLASH
	struct ota_storage *storage_ext;
#endif

	int data_buf_size;
	uint8_t *data_buf;
	uint32_t xml_offset;
	struct ota_manifest manifest;
	struct ota_breakpoint bp;
	struct ota_rx_info rx_info;

	u8_t bp_reported;
	u8_t data_buf_owner;
	uint16_t xfer_time;
	uint32_t temp_image_offset;
	const char *public_key;
};

static int ota_update_state(struct ota_upgrade_info *ota, enum ota_state state)
{
    int old_state = OTA_STATE_MAX;

	SYS_LOG_INF("%d", state);

	if (state < OTA_STATE_MAX) {
		old_state = ota->state;
		ota->state = state;
	}

	if (ota->notify) {
		return ota->notify(state, old_state);
	}

	return 0;
}

static struct ota_storage *ota_get_part_storage(struct ota_upgrade_info *ota, int storage_id)
{
	int cur_storage_id = ota_storage_get_storage_id(ota->storage);

	if(storage_id == cur_storage_id){
		return ota->storage;
	}

#ifdef 	CONFIG_OTA_DOWNLOAD_EXTERNAL_FLASH
	cur_storage_id = ota_storage_get_storage_id(ota->storage_ext);
	if(storage_id == cur_storage_id){
		return ota->storage_ext;
	}
#endif

	return NULL;
}


static int ota_partition_erase_part(struct ota_upgrade_info *ota,
			     const struct partition_entry *part,
			     int start_offset, int initial_state)
{
	int err, align_addr, align_size;
	char part_name[9];
	struct ota_storage *storage = ota_get_part_storage(ota, part->storage_id);

	if(!storage){
		return -EINVAL;
	}

	/* name field to str */
	memcpy(part_name, part->name, 8);
	part_name[8] = '\0';

	SYS_LOG_INF("part %s: (0x%x, 0x%x, 0x%x)",
		part_name, part->offset, part->size, start_offset);

	align_addr = ROUND_DOWN(part->offset + start_offset, OTA_ERASE_ALIGN_SIZE);
	align_size = ROUND_UP(part->size - start_offset, OTA_ERASE_ALIGN_SIZE);

	SYS_LOG_INF("aligned offset 0x%x, size 0x%x",
		align_addr, align_size);

	if(!initial_state){
		err = ota_storage_erase(storage, align_addr, align_size);
	}else{
		if(align_size > OTA_ERASE_ALIGN_SIZE){
			//first not erase first align addr data
			err = ota_storage_erase_initial(storage, align_addr + OTA_ERASE_ALIGN_SIZE, align_size - OTA_ERASE_ALIGN_SIZE);
			err |= ota_storage_erase_initial(storage, align_addr, OTA_ERASE_ALIGN_SIZE);
		}else{
			err = ota_storage_erase_initial(storage, align_addr, align_size);
		}
	}
	if (err) {
		return err;
	}

	return 0;
}

int ota_partition_check_part_is_clean(struct ota_upgrade_info *ota, const struct partition_entry *part)
{
	int is_clean = true;
	uint8_t check_buff[512];
	int i;
	struct ota_storage *storage = ota_get_part_storage(ota, part->storage_id);

	if(!storage){
		return false;
	}

	if (ota_use_recovery_app(ota)){
		return false;
	}

	for(i = 0; i < 4; i++){
		if(part->size < (i * 0x10000)){
			SYS_LOG_WRN("storage off %x, size 0x%x\n", (i * 0x10000), part->size);
			break;
		}

		is_clean = ota_storage_is_clean(storage, part->offset + (i * 0x10000), OTA_ERASE_ALIGN_SIZE,
			check_buff, sizeof(check_buff));

		if (is_clean != 1) {
			SYS_LOG_INF("storage not clean, offs 0x%x size 0x%x\n", part->offset, part->size);
			return false;
		}
	}

	return is_clean;
}

int ota_partition_update_prepare_init(struct ota_upgrade_info *ota)
{
	struct ota_breakpoint *bp = &ota->bp;

	const struct partition_entry *part;
	int i;

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		part = partition_get_part_by_id(i);
		if (part == NULL)
			return -EINVAL;

		if (part->file_id == 0)
			continue;

		if (!ota_use_recovery(ota)) {
			/* skip current firmware's partitions */
			if (!partition_is_mirror_part(part)) {
				SYS_LOG_INF("part[%d]: skip current partition", i);
				continue;
			}

			if (part->file_id == PARTITION_FILE_ID_EVTBUF || part->file_id == PARTITION_FILE_ID_COREDUMP){
				continue;
			}
		} else {
			if (ota_use_recovery_app(ota)){
				/* don't erase partition that not in current storage */
				if (ota_get_part_storage(ota, part->storage_id) == NULL) {
					SYS_LOG_INF("part[%d]: skip not current storage %d", i, part->storage_id);
					continue;
				}

				/* only temp partition need be erased when recovery is enabled */
				if (part->file_id != PARTITION_FILE_ID_SYSTEM &&
				    part->file_id != PARTITION_FILE_ID_SDFS) {
					SYS_LOG_INF("part[%d]: skip not recovery app", part->file_id);
					continue;
				}
			}else{
				/* don't erase partition that not in current storage */
				if (ota_get_part_storage(ota, part->storage_id) == NULL) {
					SYS_LOG_INF("part[%d]: skip not current storage %d", i, part->storage_id);
					continue;
				}

				/* only temp partition need be erased when recovery is enabled */
				if (part->type != PARTITION_TYPE_TEMP &&
				    part->file_id != PARTITION_FILE_ID_OTA_TEMP) {
					SYS_LOG_INF("part[%d]: skip not temp", i);
					continue;
				}
			}
		}

		if(!ota_partition_check_part_is_clean(ota, part)){
			ota_partition_erase_part(ota, part, 0, true);
		}

		ota_breakpoint_set_file_state(bp, part->file_id, OTA_BP_FILE_STATE_CLEAN);
	}

	bp->state = OTA_BP_STATE_CLEAN;
	SYS_LOG_INF("bp state is clean");

	ota_breakpoint_save(bp);

	return 0;
}


int ota_partition_update_prepare(struct ota_upgrade_info *ota, int init_state)
{
	struct ota_breakpoint *bp = &ota->bp;
	const struct partition_entry *part;
	int i, file_state, erase_offset;

	SYS_LOG_INF("bp_state %d", bp->state);

	if (bp->state == OTA_BP_STATE_CLEAN) {
		SYS_LOG_INF("bp_state clean, skip erase parts");
		return 0;
	}

	//skip partition erase because temp partition is valid data and need upgrade
	if (ota_use_recovery_app(ota) && bp->state == OTA_BP_STATE_UPGRADE_DONE) {
		/* state is clean, skip erase */
		SYS_LOG_INF("bp_state done, skip erase parts");
		return 0;
	}

	if (ota_use_recovery(ota)) {
		if (bp->state == OTA_BP_STATE_UPGRADE_PENDING) {
			/* state is clean, skip erase */
			SYS_LOG_INF("upgrade pending, skip erase parts");
			return 0;
		}

		/* don't erase temp part is upgrading is going, it will be erased before write file */
		if (bp->state == OTA_BP_STATE_UPGRADE_WRITING ||
		    bp->state == OTA_BP_STATE_UPGRADING_FAIL) {
			SYS_LOG_INF("upgrading, skip erase temp part");
			return 0;
		}
	}

	ota_update_state(ota, OTA_UPGRADE_PREPARE);

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		part = partition_get_part_by_id(i);
		if (part == NULL)
			return -EINVAL;

		if (part->file_id == 0)
			continue;

		if (!ota_use_recovery(ota)) {
			/* skip current firmware's partitions */
			if (!partition_is_mirror_part(part)) {
				SYS_LOG_INF("part[%d]: skip current partition", i);
				continue;
			}

			if (part->file_id == PARTITION_FILE_ID_EVTBUF || part->file_id == PARTITION_FILE_ID_COREDUMP){
				continue;
			}
		} else {
			/* only temp partition need be erased when recovery is enabled */
			if (part->type != PARTITION_TYPE_TEMP &&
			    part->file_id != PARTITION_FILE_ID_OTA_TEMP) {
				SYS_LOG_INF("part[%d]: skip not temp", i);
				continue;
			}

			/* don't erase partition that not in current storage */
			if (ota_get_part_storage(ota, part->storage_id) == NULL) {
				SYS_LOG_INF("part[%d]: skip not current storage %d", i, part->storage_id);
				continue;
			}

			if (ota_use_recovery_app(ota) && part->type == PARTITION_TYPE_TEMP){
				SYS_LOG_INF("skip erase temp");
				continue;
			}
		}

		printk("\n");
		if (bp->state == OTA_BP_STATE_UPGRADE_WRITING ||
		    bp->state == OTA_BP_STATE_WRITING_IMG || bp->state == OTA_BP_STATE_UPGRADE_WRITING_OTHER) {
			/* check breakpoint */
			file_state = ota_breakpoint_get_file_state(bp, part->file_id);

			SYS_LOG_INF("part[%d]: bp_state %d, file_id %d, file_state %d", i, bp->state, part->file_id, file_state);

			if (file_state == OTA_BP_FILE_STATE_CLEAN ||
			    file_state == OTA_BP_FILE_STATE_WRITE_DONE ||
			    file_state == OTA_BP_FILE_STATE_VERIFY_PASS ||
			    file_state == OTA_BP_FILE_STATE_WRITING_CLEAN) {
				printk("skip erase\n");
				continue;
			} else if (file_state == OTA_BP_FILE_STATE_WRITING || file_state == OTA_BP_FILE_STATE_OTHER_WRITING) {
				if (ota_use_recovery(ota) ||
					(bp->mirror_id == !partition_get_current_mirror_id())) {
					/* parition is writing, not clean */
					erase_offset = bp->cur_file.offset + bp->cur_file_write_offset;
					erase_offset &= ~(OTA_ERASE_ALIGN_SIZE - 1);
					erase_offset -= part->offset;

					printk("file_offs=0x%x, bp_offs=0x%x, erase_from 0x%x\n",
						bp->cur_file.offset, bp->cur_file_write_offset, erase_offset);

					/* update write offset aligned with erase sector */
					bp->cur_file_write_offset = part->offset + erase_offset - bp->cur_file.offset;

					ota_partition_erase_part(ota, part, erase_offset, init_state);
					ota_breakpoint_set_file_state(bp, part->file_id, OTA_BP_FILE_STATE_WRITING_CLEAN);
					continue;
				}
			}
		}

		ota_partition_erase_part(ota, part, 0, init_state);
		ota_breakpoint_set_file_state(bp, part->file_id, OTA_BP_FILE_STATE_CLEAN);

		if(bp->state == OTA_BP_STATE_WRITING_IMG_FAIL && part->file_id == PARTITION_FILE_ID_OTA_TEMP){
			bp->state = OTA_BP_STATE_CLEAN;
		}
	}

	ota_update_state(ota, OTA_UPGRADE_PREPARE_DONE);

	//sleep for log message output
	os_sleep(10);

	if (bp->state != OTA_BP_STATE_UPGRADE_WRITING &&
	    bp->state != OTA_BP_STATE_WRITING_IMG && bp->state != OTA_BP_STATE_CLEAN) {
		if (bp->state != OTA_BP_STATE_UNKOWN) {
			/* clear all old status */
			SYS_LOG_INF("clear old bp status");
			ota_breakpoint_init_default_value(&ota->bp);
		}

		bp->state = OTA_BP_STATE_CLEAN;
		SYS_LOG_INF("bp state is clean");
	}

	ota_breakpoint_save(bp);

	return 0;
}

static int ota_check_storage_file_crc(struct ota_upgrade_info *ota, struct ota_storage *storage, struct ota_file *file)
{
	int crc_calc, crc_orig;

	SYS_LOG_INF("%s: addr 0x%x, size 0x%x", file->name, file->offset, file->size);

	crc_calc = ota_storage_calc_crc(storage, file->offset, file->size, ota->data_buf, ota->data_buf_size);

	crc_orig = file->checksum;

	SYS_LOG_INF("%s: orig 0x%x, calc 0x%x", file->name, crc_orig, crc_calc);
	if (crc_calc != crc_orig) {
		print_buffer(ota->data_buf, 1, file->size, 16, -1);
		return -1;
	}

	return 0;

}


static int ota_secure_data_verify(struct ota_upgrade_info *ota, struct ota_storage *storage, struct ota_file *file)
{
	int check_ret;

	printk("RSA verify data from %x to %x, signature offset %x len %x\n", file->offset, file->offset + file->size - OTA_SIGNATURE_DATA_LEN,\
			file->offset + file->size - OTA_SIGNATURE_DATA_LEN, OTA_SIGNATURE_DATA_LEN);

	check_ret = ota_storage_check_image_sig_data(storage, ota->public_key, file->offset, file->size - OTA_SIGNATURE_DATA_LEN,\
		file->offset + file->size - OTA_SIGNATURE_DATA_LEN, OTA_SIGNATURE_DATA_LEN, ota->data_buf, ota->data_buf_size);

	if(check_ret != 0){
		printk("RSA signature check failed %x\n", check_ret);
	}else{
		printk("RSA signature check ok");
	}

	return check_ret;
}

static int ota_encrypt_data_verify(struct ota_upgrade_info *ota, struct ota_file *file)
{
	uint32_t crc_calc, crc_orig;
	uint32_t mapping_addr = 0xc00000;

	soc_memctrl_mapping(mapping_addr, file->offset, false);

	crc_calc = utils_crc32(0, (uint8_t *)mapping_addr, file->size);

	crc_orig = file->checksum;

	soc_memctrl_clear_temp_mapping((void *)mapping_addr);

	SYS_LOG_INF("%s: orig 0x%x, calc 0x%x", file->name, crc_orig, crc_calc);
	if (crc_calc != crc_orig) {
		return -1;
	}

	return 0;
}

static int ota_update_temp_partition_flag(struct ota_upgrade_info *ota)
{
	uint8_t *data_ptr = (uint8_t *)ota_rx_in_buf;

	const struct partition_entry * part;

	part = partition_get_part(PARTITION_FILE_ID_OTA_TEMP);

	struct ota_storage *storage = ota_get_part_storage(ota, part->storage_id);

	if(!storage){
		return -EINVAL;
	}

	ota_storage_read(storage, part->offset, data_ptr, OTA_ERASE_ALIGN_SIZE);

	if(data_ptr[0] != 0x41 && data_ptr[1] != 0x4F \
		&& data_ptr[2] != 0x54 && data_ptr[3] != 0x41){

		data_ptr[0] = 0x41;
		data_ptr[1] = 0x4F;
		data_ptr[2] = 0x54;
		data_ptr[3] = 0x41;

		ota_storage_write(storage, part->offset, data_ptr, 4);
	}


	return 0;
}


static int ota_update_clear_temp_partition_flag(struct ota_upgrade_info *ota)
{
	uint8_t *data_ptr = (uint8_t *)ota_rx_in_buf;

	const struct partition_entry * part;

	part = partition_get_part(PARTITION_FILE_ID_OTA_TEMP);

	struct ota_storage *storage = ota_get_part_storage(ota, part->storage_id);

	if(!storage){
		return -EINVAL;
	}

	ota_storage_read(storage, part->offset, data_ptr, OTA_ERASE_ALIGN_SIZE);

	memset(data_ptr, 0, 4);

	ota_storage_write(storage, part->offset, data_ptr, 4);

	return 0;
}

static int ota_verify_file(struct ota_upgrade_info *ota, struct ota_file *file)
{
	int prio;
	uint32_t crc_calc;
	const struct partition_entry * part = partition_get_part(file->file_id);

	if(!part){
		return -1;
	}

	struct ota_storage *storage = ota_get_part_storage(ota, part->storage_id);

	if(!storage){
		return -1;
	}

	if(file->file_id == PARTITION_FILE_ID_OTA_TEMP){
		prio = k_thread_priority_get(k_current_get());

		k_thread_priority_set(k_current_get(), K_LOWEST_THREAD_PRIO - 1);

		if(ota_use_secure_boot(ota)) {
			crc_calc = ota_secure_data_verify(ota, storage, file);
		}else{
			crc_calc = ota_storage_image_check(storage, file->offset, ota->data_buf, ota->data_buf_size);
		}

		k_thread_priority_set(k_current_get(), prio);

		if(crc_calc != 0){
			//erase temp image
			ota_update_clear_temp_partition_flag(ota);
		}

		return crc_calc;
	}else if(ota_use_recovery_app(ota) && (part->flag & PARTITION_FLAG_ENABLE_ENCRYPTION)){
		return ota_encrypt_data_verify(ota, file);
	}else{

		return ota_check_storage_file_crc(ota, storage, file);
	}
}

#ifdef CONFIG_OTA_FILE_PATCH
static int ota_is_patch_fw(struct ota_upgrade_info *ota)
{
	const struct fw_version *old_fw_ver = &ota->manifest.old_fw_ver;

	return (old_fw_ver->version_code == 0) ? 0 : 1;
}

static int ota_write_file_by_patch(struct ota_upgrade_info *ota, struct ota_file *file, int start_file_offs)
{
	struct ota_image *img = ota->img;
	struct ota_storage *storage;
	int img_file_offset;
	int err=0, is_clean, patch_file_size;
	uint32_t start_time, consume_time;
	struct ota_file_patch_info file_patch;
	const struct partition_entry *part;
	void *mapping_addr;

	SYS_LOG_INF("%s offset 0x%x", file->name, file->offset);

	start_time = k_uptime_get_32();

	if (start_file_offs != 0) {
		SYS_LOG_ERR("breakpoint unsupported");
		return -EINVAL;
	}

	img_file_offset = ota_image_get_file_offset(img, file->name);
	if (img_file_offset < 0) {
		SYS_LOG_ERR("cannot found file %s in image", file->name);
		return err;
	}

	patch_file_size = ota_image_get_file_length(img, file->name);

	part = partition_get_part(file->file_id);
	if (part == NULL)
		return -EINVAL;

	storage = ota_get_part_storage(ota, part->storage_id);

	if(!storage){
		return -EINVAL;
	}

	/* check empty */
	printk("file->size 0x%x, ota->data_buf %p, data_buf_size 0x%x, part->flag 0x%x\n",
		file->size, ota->data_buf, ota->data_buf_size, part->flag);
	is_clean = ota_storage_is_clean(storage, file->offset, file->size,
		ota->data_buf, ota->data_buf_size);
	if (is_clean != 1) {
		SYS_LOG_ERR("storage not clean, offs 0x%x size 0x%x", file->offset, file->size);
		return -EINVAL;
	}

	ota_breakpoint_update_file_state(&ota->bp, file, OTA_BP_FILE_STATE_WRITING_DIRTY, 0, 0);

	mapping_addr = soc_memctrl_create_temp_mapping(part->file_offset, part->flag & PARTITION_FLAG_ENABLE_CRC);

	memset(&file_patch, 0x0, sizeof(struct ota_file_patch_info));

	file_patch.img = img;
	file_patch.storage = storage;
	file_patch.old_file_mapping_addr = mapping_addr;
	file_patch.old_file_offset = part->file_offset;
	file_patch.old_file_size = part->size;
	file_patch.new_file_offset = file->offset; // + start_file_offs;
	file_patch.new_file_size = file->size;
	file_patch.patch_file_offset = img_file_offset;
	file_patch.patch_file_size = patch_file_size;
	file_patch.flag_use_crc = (part->flag & PARTITION_FLAG_ENABLE_CRC) ? 1 : 0;
	file_patch.flag_use_encrypt = (part->flag & PARTITION_FLAG_ENABLE_ENCRYPTION) ? 1 : 0;

	file_patch.write_cache = ota->data_buf;
	file_patch.write_cache_size = 0x22;
	file_patch.write_cache_offs = 0;
	file_patch.write_cache_pos = 0;

	err = ota_file_patch_write(&file_patch);
	if (err) {
		SYS_LOG_ERR("storage write failed, offs 0x%x size 0x%x", file->offset, file->size);
		return -EIO;
	}

	consume_time = k_uptime_get_32() - start_time;
	SYS_LOG_INF("%s(%d KB, patch %d KB), cost %d ms, %d KB/s", file->name, file->size / 1024, \
		patch_file_size / 1024, consume_time, file->size / consume_time);

	soc_memctrl_clear_temp_mapping(mapping_addr);

	return 0;
}
#endif

#ifdef CONFIG_OTA_UPGRADE_SAVE_OTA_HEADER
int ota_upgrade_is_need_save_ota_header(struct ota_upgrade_info *ota)
{
	//SYS_LOG_INF("backend type %d crc %d\n", ota_upgrade_get_backend_type(ota), ota_dfu_breakpoint_get_header_crc());

	if(ota_upgrade_get_backend_type(ota) == OTA_BACKEND_TYPE_SELFAPP && ota_dfu_breakpoint_get_header_crc() == 0){
		return true;
	}

	return false;
}

int ota_upgrade_is_header_file_valid(struct ota_upgrade_info *ota)
{
	SYS_LOG_INF("backend type %d crc %d\n", ota_upgrade_get_backend_type(ota), ota_dfu_breakpoint_get_header_crc());

	if(ota_upgrade_get_backend_type(ota) == OTA_BACKEND_TYPE_SELFAPP && ota_dfu_breakpoint_get_header_crc() != 0){
		return true;
	}

	return false;
}


int ota_upgrade_save_ota_header(struct ota_upgrade_info *ota)
{
	struct ota_header_info header_info;

	ota_image_ioctl(ota->img, OTA_BACKEND_IOCTL_REPORT_HEADER_CRC_INFO, (unsigned int)&header_info);

	SYS_LOG_INF("save ota header %x packet length %d %d\n", header_info.header_crc, header_info.packet_length, header_info.header_len);

	ota_dfu_breakpoint_save_header_crc(header_info.header_crc, header_info.packet_length, header_info.header_len);

	SYS_LOG_INF();

	return 0;
}

#endif

static int ota_local_image_read(struct ota_upgrade_info *ota, int offset, uint8_t *buf, int size)
{
	const struct partition_entry * part = partition_get_part(PARTITION_FILE_ID_OTA_TEMP);

	struct ota_storage *storage = ota_get_part_storage(ota, part->storage_id);

	if(!storage){
		return -EINVAL;
	}

	printk("read file offset %x len %x\n", ota->temp_image_offset + offset, size);
	ota_storage_read(storage, ota->temp_image_offset + offset, buf, size);
	return 0;
}

static void ota_rx_thread(void *p1, void *p2, void *p3)
{
	struct ota_upgrade_info *ota = (struct ota_upgrade_info *)p1;
	struct ota_backend *backend = ota_image_get_backend(ota->img);
	struct ota_rx_info *rx_info = &ota->rx_info;
	bool is_bt_backend;
	int err, rlen, req_size, max_req_size;
	uint32_t start_time;
	const struct partition_entry * part = partition_get_part(rx_info->file_id);
	uint32_t read_local_image = false;

	is_bt_backend = (ota_backend_get_type(backend) == OTA_BACKEND_TYPE_BLUETOOTH);
	max_req_size = OTA_REQ_MAX_SIZE;
	ota_backend_ioctl(backend, OTA_BACKEND_IOCTL_GET_MAX_SIZE, (unsigned int)&max_req_size);

	if(ota_use_recovery(ota)){
		if(part && (partition_is_boot_part(part) || partition_is_param_part(part))){
			read_local_image = true;
		}
	}

	SYS_LOG_INF("ota_rx thread started file id %d %d %d", rx_info->file_id, is_bt_backend, read_local_image);

	while ((rx_info->size > 0) && (rx_info->rx_errno == 0) && (!rx_info->thread_need_terminated)) {
		req_size = rx_info->size;

		if (req_size > max_req_size) {
			req_size = max_req_size;
		}

		if (is_bt_backend && !read_local_image) {
			err = ota_image_read_prepare(ota->img, rx_info->offset, ota->data_buf, req_size);
			if (err) {
				SYS_LOG_ERR("read data  err, offs 0x%x", rx_info->offset);
				rx_info->rx_errno = -EAGAIN;
			}
		}

		while (req_size > 0) {
			if (req_size < rx_info->seg_size) {
				rlen = req_size;
			} else {
				rlen = rx_info->seg_size;
			}

			start_time = k_uptime_get_32();
			if (is_bt_backend && !read_local_image) {
				err = ota_image_read_complete(ota->img, rx_info->offset, ota->data_buf, rlen);
			} else if (read_local_image){
				err = ota_local_image_read(ota, rx_info->offset, ota->data_buf, rlen);
			}else {
				err = ota_image_read(ota->img, rx_info->offset, ota->data_buf, rlen);
			}
			ota->xfer_time += (k_uptime_get_32() - start_time);

			if (err) {
				SYS_LOG_ERR("cannot read data, offs 0x%x", rx_info->offset);
				rx_info->rx_errno = -EAGAIN;
				break;
			}

			while (acts_ringbuf_space(&rx_info->rbuf) < rlen) {
				SYS_LOG_DBG("ota_rx wait rbuf");
				os_sem_take(&rx_info->rx_put_sem, OS_FOREVER);
			}


			acts_ringbuf_put(&rx_info->rbuf, (const uint8_t *)ota->data_buf, rlen);
			os_sem_give(&rx_info->rx_get_sem);

			req_size -= rlen;
			rx_info->offset += rlen;
			rx_info->size -= rlen;
		}
	}

	SYS_LOG_INF("ota_rx thread exited");
	os_sem_give(&rx_info->rx_get_sem);
	rx_info->thread_terminated = true;
}

static int ota_rx_init(struct ota_upgrade_info *ota)
{
	struct ota_rx_info *rx_info = &ota->rx_info;

	rx_info->rx_stack = (char *)ota_rx_stack;
	rx_info->rx_buf = ota_rx_data_buf;
	rx_info->rx_bufsize = OTA_RX_BUFSIZE;
	rx_info->in_buf = ota_rx_in_buf;
	rx_info->in_bufsize = OTA_IN_BUFSIZE;

	os_sem_init(&rx_info->rx_get_sem, 0, 5);
	os_sem_init(&rx_info->rx_put_sem, 0, 1);
	acts_ringbuf_init(&rx_info->rbuf, rx_info->rx_buf, rx_info->rx_bufsize);

	return 0;
}

static int ota_rx_exit(struct ota_upgrade_info *ota)
{
	struct ota_rx_info *rx_info = &ota->rx_info;

	if (rx_info->rx_stack != NULL) {
		rx_info->rx_stack = NULL;
	}
	if (rx_info->rx_buf != NULL) {
		rx_info->rx_buf = NULL;
		rx_info->rx_bufsize = 0;
	}
	if (rx_info->in_buf != NULL) {
		rx_info->in_buf = NULL;
		rx_info->in_bufsize = 0;
	}


	return 0;
}

static void ota_rx_start(struct ota_upgrade_info *ota, uint32_t offset,
				uint32_t size, uint32_t seg_size, uint8_t file_id, bool is_raw)
{
	struct ota_rx_info *rx_info = &ota->rx_info;
	char *stack_ptr;

	rx_info->offset = offset;
	rx_info->size = size;
	rx_info->seg_size = seg_size;
	rx_info->is_raw = is_raw;
	rx_info->rx_errno = 0;
	rx_info->thread_terminated = false;
	rx_info->thread_need_terminated = false;
	rx_info->file_crc = 0;
	rx_info->file_id = file_id;

	os_sem_reset(&rx_info->rx_get_sem);
	os_sem_reset(&rx_info->rx_put_sem);
	acts_ringbuf_reset(&rx_info->rbuf);

	stack_ptr = (char *)ROUND_UP(rx_info->rx_stack, STACK_ALIGN);

	rx_info->rx_tid = (os_tid_t)k_thread_create(&rx_info->rx_info_thread, (os_thread_stack_t)stack_ptr, OTA_RX_STACKSIZE, ota_rx_thread,
									ota, NULL, NULL, 3, 0, OS_NO_WAIT);

}

static void ota_rx_stop(struct ota_upgrade_info *ota)
{
	struct ota_rx_info *rx_info = &ota->rx_info;

	rx_info->thread_need_terminated = true;
	while(!rx_info->thread_terminated){
		os_sleep(1000);
		printk("wait ota rx thread exit\n");
	}
}

static int ota_calc_write_seg_size(struct ota_upgrade_info *ota)
{
	struct ota_backend *backend;
	struct ota_image *img = ota->img;
	int unit_size = 0;

	backend = ota_image_get_backend(img);
	ota_backend_ioctl(backend, OTA_BACKEND_IOCTL_GET_UNIT_SIZE, (unsigned int)&unit_size);

	if(unit_size == 0){
		unit_size = OTA_ERASE_ALIGN_SIZE;
	}

	return (ota->data_buf_size / unit_size) * unit_size;
}




static int ota_write_file_partition(struct ota_upgrade_info *ota, struct ota_file *file, uint32_t offs, uint8_t *data, uint32_t size)
{
	int ret = 0;
	uint32_t wlen;
	uint32_t addr = file->offset + offs;
	struct ota_storage *storage;
	const struct partition_entry * part = partition_get_part(file->file_id);

	if(!part){
		return -EINVAL;
	}

	storage = ota_get_part_storage(ota, part->storage_id);

	if(!storage){
		return -EINVAL;
	}

	SYS_LOG_INF("file id %d offs 0x%x, size %d storage id %d", file->file_id, offs, size, ota_storage_get_storage_id(storage));

	if(ota_use_recovery_app(ota) && (part->flag & PARTITION_FLAG_ENABLE_ENCRYPTION)){
		addr |= (1 << 31);
		if (size % 32) {
			SYS_LOG_ERR("len %d shall align with 32 bytes", size);
			return -EINVAL;
		}

		wlen = 32;
		while (size) {
			if (size < wlen)
				wlen = size;
			ret = ota_storage_write(storage, addr, data, wlen);
			if (ret) {
				SYS_LOG_ERR("storage write failed, offs 0x%x", addr);
				return -EFAULT;
			}
			data += wlen;
			addr += wlen;
			size -= wlen;
		}
	}else{

		if(file->file_id == PARTITION_FILE_ID_OTA_TEMP && offs == 0){
			memset(data, 0xFF, 4);
		}

		ret = ota_storage_write(storage, addr, data, size);
		if (ret) {
			SYS_LOG_ERR("storage write failed, addr 0x%x", addr);
			ota_rx_stop(ota);
			return -EIO;
		}
	}

	return ret;
}

static int ota_write_file_normal(struct ota_upgrade_info *ota, struct ota_file *file, int start_file_offs)
{
	struct ota_image *img = ota->img;

	struct ota_breakpoint *bp = &ota->bp;
	struct ota_rx_info *rx_info = &ota->rx_info;
	unsigned int offs;
	int img_file_offset;
	int ret = 0, seg_size, file_len, wlen, in_size;
	uint32_t start_time, consume_time;
	bool is_record = false;
	bool no_wait = false;
	int need_save;

	SYS_LOG_INF("%s size=0x%x, offset=0x%x bpoffs=0x%x",
		file->name, file->size, file->offset, start_file_offs);

	start_time = k_uptime_get_32();
	file_len = file->size;

	if (start_file_offs >= file_len) {
		SYS_LOG_ERR("start_offs exceed 0x%x_0x%x", start_file_offs, file_len);
		return -EINVAL;
	}

	offs = start_file_offs;

	if (file->file_id == PARTITION_FILE_ID_OTA_TEMP) {
		img_file_offset = ota_image_get_file_offset(img, NULL);
		//if (ota_use_recovery(ota) && !ota_use_recovery_app(ota)) {
		//	if (!start_file_offs)
				//offs += ota->xml_offset;
		//}
	} else {
		img_file_offset = ota_image_get_file_offset(img, file->name);
	}

	if (img_file_offset < 0) {
		SYS_LOG_ERR("%s not found", file->name);
		return -EINVAL;
	}

	wlen = file_len - offs;

	seg_size = ota_calc_write_seg_size(ota);

	ota_rx_start(ota, img_file_offset + offs, wlen, seg_size, file->file_id, true);

	while (wlen > 0) {
		if (!no_wait) {
			os_sem_take(&rx_info->rx_get_sem, OS_FOREVER);
			if (rx_info->rx_errno) {
				ota_rx_stop(ota);
				need_save = ota_breakpoint_update_file_state(bp, file, OTA_BP_FILE_STATE_WRITING, offs, 1);
#ifdef CONFIG_OTA_UPGRADE_SAVE_OTA_HEADER
				if(need_save && (ota_upgrade_get_backend_type(ota) == OTA_BACKEND_TYPE_SELFAPP)){
					ota_dfu_breakpoint_set_breakpoint(offs);
				}
#endif
				return rx_info->rx_errno;
			}
		}
		no_wait = false;

		in_size = acts_ringbuf_length(&rx_info->rbuf);

		if (in_size > rx_info->in_bufsize) {
			if (in_size >= rx_info->in_bufsize * 2) {
				no_wait = true;
			}
			in_size = rx_info->in_bufsize;
		}

		if (in_size < wlen) {
			if (in_size < OTA_ERASE_ALIGN_SIZE) {
				continue;
			}
			in_size = ROUND_DOWN(in_size, OTA_ERASE_ALIGN_SIZE);
		}

		if (file->file_id == PARTITION_FILE_ID_OTA_TEMP){
			if (!is_record || (wlen == in_size)) {
				need_save = ota_breakpoint_update_file_state(bp, file, OTA_BP_FILE_STATE_WRITING, offs, 1);
				is_record = true;
			} else {
				need_save = ota_breakpoint_update_file_state(bp, file, OTA_BP_FILE_STATE_WRITING, offs, 0);
			}
#ifdef CONFIG_OTA_UPGRADE_SAVE_OTA_HEADER
			if(need_save && (ota_upgrade_get_backend_type(ota) == OTA_BACKEND_TYPE_SELFAPP)){
				ota_dfu_breakpoint_set_breakpoint(offs);
			}
#endif
		}

		ret = acts_ringbuf_get(&rx_info->rbuf, rx_info->in_buf, in_size);

		os_sem_give(&rx_info->rx_put_sem);
		if (ret != in_size) {
			SYS_LOG_ERR("ring buf get failed, size 0x%x", ret);
			ota_rx_stop(ota);
			return -EAGAIN;
		}

		rx_info->file_crc = utils_crc32(rx_info->file_crc, rx_info->in_buf, in_size);

		ret = ota_write_file_partition(ota, file, offs, rx_info->in_buf, in_size);
		if (ret) {
			ota_rx_stop(ota);
			return -EIO;
		}

		offs += in_size;
		wlen -= in_size;

#ifdef CONFIG_OTA_UPGRADE_SAVE_OTA_HEADER
		if (offs >= CONFIG_OTA_UPGRADE_SAVE_OTA_HEADER_LEN && ota_upgrade_is_need_save_ota_header(ota)){
			ota_upgrade_save_ota_header(ota);
		}
#endif
	}

	consume_time = k_uptime_get_32() - start_time;

	SYS_LOG_INF("%s(%d KB), cost %d ms, %d KB/s", file->name, file_len / 1024,
		consume_time, file_len / consume_time);

	SYS_LOG_INF("file crc %x get crc %x", file->checksum, rx_info->file_crc);

	ota_rx_stop(ota);

	return 0;
}

static int ota_write_file(struct ota_upgrade_info *ota, struct ota_file *file, int start_file_offs)
{
#ifdef CONFIG_OTA_FILE_PATCH
	if (ota_is_patch_fw(ota)) {
		return ota_write_file_by_patch(ota, file, start_file_offs);
	} else {
#endif
		return ota_write_file_normal(ota, file, start_file_offs);
#ifdef CONFIG_OTA_FILE_PATCH
	}
#endif
}



static int ota_write_and_verify_file(struct ota_upgrade_info *ota,
				     const struct partition_entry *part,
				     struct ota_file *file, bool need_verify)
{
	struct ota_breakpoint *bp = &ota->bp;
	int bp_file_state, start_write_offset = 0;
	int err = 0, need_erase = 0;

	bp_file_state = ota_breakpoint_get_file_state(bp, file->file_id);

	SYS_LOG_INF("%s: file_id %d, bp_file_state %d", file->name, file->file_id, bp_file_state);

	switch (bp_file_state) {
	case OTA_BP_FILE_STATE_WRITE_DONE:
	case OTA_BP_FILE_STATE_VERIFY_PASS:
		SYS_LOG_INF("already write done");
		break;

	case OTA_BP_FILE_STATE_OTHER_WRITING:
		SYS_LOG_INF("other writing");
		break;

	case OTA_BP_FILE_STATE_CLEAN:
		SYS_LOG_INF("part clean");
		break;
	case OTA_BP_FILE_STATE_WRITING_CLEAN:
		SYS_LOG_INF("part clean, write offset 0x%x", bp->cur_file_write_offset);
		start_write_offset = bp->cur_file_write_offset;
		break;
	case OTA_BP_FILE_STATE_WRITING:
		SYS_LOG_INF("part not clean, write offset 0x%x", bp->cur_file_write_offset);
		start_write_offset = bp->cur_file_write_offset;
		need_erase = 1;
		break;
	default:
		SYS_LOG_INF("write offset 0 by default");
		need_erase = 1;
		break;
	}

	if (ota_get_part_storage(ota, part->storage_id) == NULL) {
		SYS_LOG_ERR("BUG: storage_id not match %d", part->storage_id);
		err = -EINVAL;
		goto failed;
	}

	if (ota_use_recovery(ota)) {
		/* we can erase flash in recovery app */
		if (need_erase) {
			int erase_offset;

			//if (!ota_use_recovery_app(ota)) {
				/* cannot erase flash if not in recovery app or single nor */
				//if (cur_storage_id == 0) {
				//	SYS_LOG_INF("update storage %d is xip, skip erase", cur_storage_id);
				//	goto skip_erase;
				//}
			//}

			erase_offset = ROUND_DOWN(file->offset + start_write_offset, OTA_ERASE_ALIGN_SIZE);
			start_write_offset = erase_offset - file->offset;

			ota_partition_erase_part(ota, part, erase_offset - part->offset, false);

			SYS_LOG_INF("write_offset from 0x%x to 0x%x", bp->cur_file_write_offset, start_write_offset);
			bp->cur_file_write_offset = start_write_offset;
		}
	}
//skip_erase:

#ifdef OTA_BPREPORT
	if (ota->bp_reported == 0) {
		ota->bp_reported = 1;
		// check if the 1st file is writing or finished
		if (start_write_offset > 0 \
		 || bp_file_state == OTA_BP_FILE_STATE_WRITE_DONE \
		 || bp_file_state == OTA_BP_FILE_STATE_VERIFY_PASS)
		{
			// bp_offset is the 1st image_read offset
			ota_update_state(ota, OTA_BREAKPOINT_REPORT);
		}
	}
#endif

	if (bp_file_state != OTA_BP_FILE_STATE_WRITE_DONE &&
	    bp_file_state != OTA_BP_FILE_STATE_VERIFY_PASS) {
		ota_breakpoint_update_file_state(bp, file, OTA_BP_FILE_STATE_WRITE_START, start_write_offset, 0);

		ota_update_state(ota, OTA_FILE_WRITE);
		err = ota_write_file(ota, file, start_write_offset);
		ota_update_state(ota, OTA_FILE_WRITE_DONE);
		if (err) {
			SYS_LOG_ERR("%s failed", file->name);
			goto failed;
		}

		ota_breakpoint_update_file_state(bp, file, OTA_BP_FILE_STATE_WRITE_DONE, 0, 0);
	}

	if (need_verify) {
		ota_update_state(ota, OTA_FILE_VERIFY);
		err = ota_verify_file(ota, file);
		ota_update_state(ota, OTA_FILE_VERIFY_DONE);
		if (err) {
			SYS_LOG_ERR("%s verify failed", file->name);
			ota_breakpoint_update_file_state(bp, file, OTA_BP_FILE_STATE_VERIFY_FAIL, 0, 0);
			goto failed;
		}

		SYS_LOG_INF("%s verify pass", file->name);
		ota_breakpoint_update_file_state(bp, file, OTA_BP_FILE_STATE_VERIFY_PASS, 0, 0);
	}

	return 0;

failed:
	if (err != -EIO && err != -EAGAIN) {
		/* we assume -EIO error that can be resumed */
		ota_breakpoint_update_file_state(bp, file, OTA_BP_FILE_STATE_WRITE_FAIL, 0, 0);
	}

	return err;
}

static int ota_upgrade_verify_along(struct ota_upgrade_info *ota)
{
	const struct partition_entry *part;
	struct ota_manifest *manifest = &ota->manifest;
	struct ota_file *file;
	int i, err;
	int cur_file_id;

	for (i = 0; i < manifest->file_cnt; i++) {
		file = &manifest->wfiles[i];

		part = partition_get_mirror_part(file->file_id);
		if (part == NULL) {
			SYS_LOG_INF("no mirror part entry for file_id %d", file->file_id);

			if (ota_use_recovery(ota)) {
				cur_file_id = partition_get_current_file_id();
				part = partition_get_part(file->file_id);
				if (cur_file_id == file->file_id || part == NULL) {
					SYS_LOG_ERR("no part entry for file_id=%d, cur_file_id=%d", file->file_id, cur_file_id);
					continue;
				}
				SYS_LOG_INF("found part entry for file_id=%d, cur_file_id=%d", file->file_id, cur_file_id);
			} else {
				return -EINVAL;
			}
		}

		/* ignore boot partition */
		if (partition_is_boot_part(part))
			continue;

		if (partition_is_param_part(part))
			continue;

		ota_update_state(ota, OTA_FILE_VERIFY);
		err = ota_verify_file(ota, file);
		ota_update_state(ota, OTA_FILE_VERIFY_DONE);
		if (err) {
			SYS_LOG_ERR("%s verify failed", file->name);
			ota_breakpoint_update_file_state(&ota->bp, file, OTA_BP_FILE_STATE_VERIFY_FAIL, 0, 0);
			return -1;
		}

		SYS_LOG_INF("%s verify pass", file->name);
		ota_breakpoint_update_file_state(&ota->bp, file, OTA_BP_FILE_STATE_VERIFY_PASS, 0, 0);
	}

	return 0;
}

static const char temp_bin_name[] = "TEMP.bin";

static int ota_init_temp_file_data(struct ota_upgrade_info *ota, struct ota_file *file, const struct partition_entry *part)
{
	memset(file, 0x0, sizeof(struct ota_file));

	if(ota_use_recovery_app(ota)){

		struct ota_storage *storage = ota_get_part_storage(ota, part->storage_id);

		if(!storage){
			return -EINVAL;
		}

		file->size = ota_storage_get_image_size(storage, part->offset);

		if(file->size < 0){
			return -EINVAL;
		}

		if(ota_use_secure_boot(ota)) {
			file->size += OTA_SIGNATURE_DATA_LEN;
		}
	}else{
		file->size = ota_upgrade_get_temp_img_size(ota);
	}


	file->file_id = PARTITION_FILE_ID_OTA_TEMP;

	memcpy(file->name, temp_bin_name, strlen(temp_bin_name));

	if (file->size >= part->size) {
		SYS_LOG_ERR("temp partition size too small 0x%x_0x%x", part->size, file->size);
		return -EINVAL;
	}

	file->offset = part->offset;

	return 0;
}

int ota_upgrade_verify_temp_image(struct ota_upgrade_info *ota)
{
	struct ota_file *file;
	struct ota_file tmp_file;
	int err;

	file = &tmp_file;

	SYS_LOG_INF();

	const struct partition_entry *part = partition_get_temp_part();

	if (part == NULL) {
		SYS_LOG_ERR("temp partition err");
		return -EINVAL;
	}

	if(ota_init_temp_file_data(ota, file, part) != 0){
		SYS_LOG_ERR("init file err %s", file->name);
		return -EINVAL;
	}

	ota_update_state(ota, OTA_FILE_VERIFY);
	err = ota_verify_file(ota, file);
	ota_update_state(ota, OTA_FILE_VERIFY_DONE);
	if (err) {
		SYS_LOG_ERR("%s verify failed", file->name);
		ota_breakpoint_update_file_state(&ota->bp, file, OTA_BP_FILE_STATE_VERIFY_FAIL, 0, 0);
		return -EIO;
	}

	SYS_LOG_INF("%s verify pass", file->name);
	ota_breakpoint_update_file_state(&ota->bp, file, OTA_BP_FILE_STATE_VERIFY_PASS, 0, 0);

	return 0;
}


static int ota_auto_update_version(struct ota_upgrade_info *ota,
				     const struct partition_entry *part,
				     struct ota_file *file)
{
	struct ota_storage *storage;
	const struct fw_version *cur_ver;
	struct fw_version *new_ver;
	uint32_t start_time, consume_time;
	uint32_t addr, len = file->size, wlen;
	uint8_t *param_ptr, *temp_param_ptr, *param_map_ptr;

	SYS_LOG_INF("file %s len %d to offset 0x%x", file->name, len, file->offset);

	storage = ota_get_part_storage(ota, part->storage_id);

	if(!storage){
		return -EINVAL;
	}

	start_time = k_uptime_get_32();
	cur_ver = (struct fw_version *)fw_version_get_current();

	param_ptr = (uint8_t *)mem_malloc(file->size);
	if (!param_ptr) {
		SYS_LOG_INF("malloc %d fail", file->size);
		return -ENOMEM;
	}
	temp_param_ptr = param_ptr;

	param_map_ptr = soc_memctrl_create_temp_mapping(file->offset, false);
	memcpy(param_ptr, param_map_ptr, file->size);
	soc_memctrl_clear_temp_mapping(param_map_ptr);


	new_ver = (struct fw_version *)(param_ptr + SOC_BOOT_FIRMWARE_VERSION_OFFSET);
	/* Allow ota upgrade even though new ota version is bigger than the current's */
	if (new_ver->version_code > cur_ver->version_code) {
		SYS_LOG_WRN("new version bigger 0x%x_0x%x", cur_ver->version_code, new_ver->version_code);
	}
	new_ver->version_code = cur_ver->version_code + 1;
#if OTA_FWVERSION_LIMIT_100
	if ((new_ver->version_code & 0xFF) >= 0x64) {
		new_ver->version_code += 0x9C;  // 0x100 = 0x64 + 0x9C
	}
	if ((new_ver->version_code & 0xFF) >= 0x64) {
		new_ver->version_code += 0x9C;  // 0x100 = 0x64 + 0x9C
	}

	if ((new_ver->version_code & 0xFF00) >= 0x6400) {
		new_ver->version_code += 0x9C00;  // 0x10000 = 0x6400 + 0x9C00
	}
	if ((new_ver->version_code & 0xFF00) >= 0x6400) {
		new_ver->version_code += 0x9C00;  // 0x10000 = 0x6400 + 0x9C00
	}

	if ((new_ver->version_code & 0xFF0000) >= 0x640000) {
		new_ver->version_code += 0x9C0000;  // 0x1000000 = 0x640000 + 0x9C0000
	}
	if ((new_ver->version_code & 0xFF0000) >= 0x640000) {
		new_ver->version_code += 0x9C0000;  // 0x1000000 = 0x640000 + 0x9C0000
	}
#endif

	new_ver->checksum = utils_crc32(0, (const uint8_t *)new_ver, sizeof(struct fw_version) - 4);

	SYS_LOG_INF("version_code 0x%06x -> 0x%06x", cur_ver->version_code, new_ver->version_code);
	SYS_LOG_INF("version_name:");
	printk("%s\n", cur_ver->version_name);
	printk("%s\n", new_ver->version_name);

	addr = file->offset;
	/* enable encryption function */
	if (part->flag & PARTITION_FLAG_ENABLE_ENCRYPTION) {
		SYS_LOG_INF("enable encryption write");
		addr |= (1 << 31);
		if (len % 32) {
			SYS_LOG_ERR("len %d shall align with 32 bytes", len);
			mem_free(temp_param_ptr);
			return -EINVAL;
		}
	}

	ota_partition_erase_part(ota, part, 0, false);

	wlen = 32;
	while (len) {
		if (len < wlen)
			wlen = len;
		if (ota_storage_write(storage, addr, param_ptr, wlen)) {
			SYS_LOG_ERR("storage write failed, offs 0x%x", addr);
			mem_free(temp_param_ptr);
			return -EFAULT;
		}
		param_ptr += wlen;
		addr += wlen;
		len -= wlen;
	}

	consume_time = k_uptime_get_32() - start_time;
	SYS_LOG_INF("write %s(%d KB) cost %d ms, %d KB/s", file->name, len / 1024,
				consume_time, len / consume_time);

	mem_free(temp_param_ptr);
	return 0;
}

static int ota_do_upgrade(struct ota_upgrade_info *ota)
{
	const struct partition_entry *part, *boot_part = NULL, *param_part = NULL;
	struct ota_manifest *manifest = &ota->manifest;
	struct ota_file *file, *boot_file = NULL, *param_file = NULL;
	int i, err, max_file_size;
	int cur_file_id;

	SYS_LOG_INF("ota file_cnt %d", manifest->file_cnt);

	for (i = 0; i < manifest->file_cnt; i++) {
		file = &manifest->wfiles[i];
		printk("\n");
		part = partition_get_mirror_part(file->file_id);
		if (part == NULL) {
			SYS_LOG_INF("no mirror part entry for file_id=%d", file->file_id);

			if (ota_use_recovery(ota)) {
				cur_file_id = partition_get_current_file_id();
				part = partition_get_part(file->file_id);
				if (cur_file_id == file->file_id || part == NULL) {
					SYS_LOG_ERR("no part entry for file_id=%d, cur_file_id=%d", file->file_id, cur_file_id);
					continue;
				}
				SYS_LOG_INF("found part entry for file_id=%d, cur_file_id=%d", file->file_id, cur_file_id);
			} else {
				return -EINVAL;
			}
		}

		max_file_size = partition_get_max_file_size(part);
		if (file->size > max_file_size) {
			SYS_LOG_ERR("part %s: file_size exceed 0x%x_0x%x", part->name, file->size, max_file_size);
			return -EINVAL;
		}

		SYS_LOG_INF("[%d]%s, file_id=%d write to nor address 0x%x", i, file->name, file->file_id, part->file_offset);
		file->offset = part->file_offset;

		if (partition_is_boot_part(part)) {
			boot_file = file;
			boot_part = part;
			continue;
		}

		if (partition_is_param_part(part)) {
			param_file = file;
			param_part = part;
			continue;
		}

		err = ota_write_and_verify_file(ota, part, file, false);
		if (err) {
			return err;
		}
	}

	err = ota_upgrade_verify_along(ota);
	if (err) {
		return err;
	}

	/* write boot file at secondly last */
	if (boot_file && boot_part) {
		/* write boot file at mirror part */
		err = ota_write_and_verify_file(ota, boot_part, boot_file, true);
		if (err) {
			return err;
		}
	}

	/* write param file at last */
	if (param_file && param_part) {
		/* write param file at mirror part */
		err = ota_write_and_verify_file(ota, param_part, param_file, true);
		if (err) {
			return err;
		}
		if (ota_use_no_version_control(ota)) {
			err = ota_auto_update_version(ota, param_part, param_file);
			if (err) {
				return err;
			}
		}
	}

	return 0;
}

int ota_boot_image_is_verify_done(struct ota_upgrade_info *ota, struct ota_file *file)
{
	struct ota_breakpoint *bp = &ota->bp;
	int bp_file_state;

	bp_file_state = ota_breakpoint_get_file_state(bp, file->file_id);

	if(bp_file_state == OTA_BP_FILE_STATE_VERIFY_PASS){
		return true;
	}else{
		return false;
	}
}

static int ota_write_boot_image(struct ota_upgrade_info *ota)
{
	const struct partition_entry *part, *boot_part = NULL, *param_part = NULL;
	struct ota_manifest *manifest = &ota->manifest;
	struct ota_file *file, *boot_file = NULL, *param_file = NULL;
	int i, err;

	fw_version_dump((struct fw_version *)fw_version_get_current());

	for (i = 0; i < manifest->file_cnt; i++) {
		file = &manifest->wfiles[i];
		part = partition_get_boot_mirror_part(file->file_id);

		if(!part){
			continue;
		}

		file->offset = part->file_offset;
		if (partition_is_boot_part(part)) {
			boot_file = file;
			boot_part = part;
			continue;
		}

		if (partition_is_param_part(part)) {
			param_file = file;
			param_part = part;
			continue;
		}
	}

	/* write boot file at secondly last */
	if (boot_file && boot_part) {

		//force erase boot file partition
		ota_breakpoint_update_file_state(&ota->bp, boot_file, OTA_BP_FILE_STATE_UNKOWN, 0, 0);

		/* write boot file at mirror part */
		err = ota_write_and_verify_file(ota, boot_part, boot_file, true);
		if (err) {
			return err;
		}
	}

	/* write param file at last */
	if (param_file && param_part) {

		//force erase param file partition
		ota_breakpoint_update_file_state(&ota->bp, param_file, OTA_BP_FILE_STATE_UNKOWN, 0, 0);

		/* write param file at mirror part */
		err = ota_write_and_verify_file(ota, param_part, param_file, true);
		if (err) {
			return err;
		}
		if (ota_use_no_version_control(ota)) {
			err = ota_auto_update_version(ota, param_part, param_file);
			if (err) {
				return err;
			}
		}
	}

	return 0;
}


static int ota_write_temp_img(struct ota_upgrade_info *ota)
{
	const struct partition_entry *temp_part;
	int err;
	struct ota_file tmp_file;
	struct ota_file *file;

	file = &tmp_file;

	temp_part = partition_get_temp_part();

	if (temp_part == NULL) {
		SYS_LOG_ERR("temp partition err");
		return -EINVAL;
	}

	if(ota_init_temp_file_data(ota, file, temp_part) != 0){
		SYS_LOG_ERR("init file err %s", file->name);
		return -EINVAL;
	}


	ota->temp_image_offset = file->offset;

	SYS_LOG_INF("file %s, file_id %d write to nor addr 0x%x",
		strlen(file->name) ? (const char *)file->name : (const char *)"<NULL>", file->file_id, file->offset);

	err = ota_write_and_verify_file(ota, temp_part, file, false);
	if (err) {
		SYS_LOG_ERR("failed to write ota image");
		return err;
	}

	return 0;
}

static int ota_is_need_upgrade(struct ota_upgrade_info *ota)
{
	struct ota_breakpoint *bp = &ota->bp;
	const struct fw_version *cur_ver, *img_ver;
	struct ota_backend *backend;
	int backend_type;

#ifdef CONFIG_OTA_FILE_PATCH
	const struct fw_version *patch_old_ver;
	patch_old_ver = &ota->manifest.old_fw_ver;
	if (patch_old_ver->magic != 0) {
		SYS_LOG_INF("\npatch_old_ver");
		fw_version_dump(patch_old_ver);
	}
#endif

	img_ver = &ota->manifest.fw_ver;
	cur_ver = fw_version_get_current();

	SYS_LOG_INF("cur_ver");
	fw_version_dump(cur_ver);

	SYS_LOG_INF("img_ver");
	fw_version_dump(img_ver);

	backend = ota_image_get_backend(ota->img);
	backend_type = ota_backend_get_type(backend);

	if (backend_type != OTA_BACKEND_TYPE_TEMP_PART &&
	    bp->backend_type != OTA_BACKEND_TYPE_UNKNOWN &&
	    bp->backend_type != backend_type) {
		SYS_LOG_WRN("backend_type (%d->%d)", bp->backend_type, backend_type);
		return -1;
	}

	if (strcmp(cur_ver->board_name, img_ver->board_name)) {
		/* skip */
		SYS_LOG_WRN("unmatched board name\n");
		return -1;
	}

#ifdef CONFIG_OTA_FILE_PATCH
	if (ota_is_patch_fw(ota)) {
		/* validate ota patch firmware version */
		if (cur_ver->version_code != patch_old_ver->version_code) {
			SYS_LOG_WRN("version unmatched (0x%x -> 0x%x)\n",
				cur_ver->version_code, ota->manifest.old_fw_ver.version_code);
			return -1;
		}

		if (ota_use_no_version_control(ota)) {
			SYS_LOG_WRN("no version control\n");
			return -1;
		}
	}
#endif

	if ((bp->state == OTA_BP_STATE_WRITING_IMG ||
	     bp->state == OTA_BP_STATE_UPGRADE_WRITING ||
	     bp->state == OTA_BP_STATE_UPGRADE_PENDING)){
	     if((bp->new_version != 0 &&
	     bp->new_version != img_ver->version_code)\
	     	|| (bp->data_checksum != 0 && (bp->data_checksum != ota_image_get_checksum(ota->img)))) {
			/* FIXME: has new version fw, need erase partition */
			SYS_LOG_WRN("new version exists(0x%x -> 0x%x)\n",
				bp->new_version, img_ver->version_code);
			return 2;
		}
	}

	if (!ota_use_no_version_control(ota)) {
		if (cur_ver->version_code >= img_ver->version_code) {
			/* skip */
			SYS_LOG_WRN("version unmatched (0x%x -> 0x%x)\n",
				cur_ver->version_code, img_ver->version_code);
			return 0;
		}
	}

	return 1;
}

//Do not do xml writing.
#if 0
static int ota_upgrade_write_xml(struct ota_upgrade_info *ota)
{
	uint8_t *fw_head = NULL, *manifest_data;
	uint32_t fw_len, manifest_len;
	const struct partition_entry *temp_part;
	struct ota_breakpoint *bp = &ota->bp;
	int err;

	temp_part = partition_get_temp_part();
	if (temp_part == NULL) {
		SYS_LOG_ERR("cannot found temp partition to store ota fw");
		return -EINVAL;
	}

	SYS_LOG_INF("bp->state %d", bp->state);

	if (bp->state == OTA_BP_STATE_UPGRADE_WRITING ||
		bp->state == OTA_BP_STATE_WRITING_IMG) {
		SYS_LOG_INF("skip write xml");
		return 0;
	}

	fw_head = ota_image_get_fw_head(ota->img, &fw_len);
	if (fw_head) {
		if (fw_len >= temp_part->size) {
			SYS_LOG_ERR("temp partition size 0x%x is too small 0x%x", temp_part->size, fw_len);
			return -EINVAL;
		}

		if (ota_storage_is_clean(ota->storage, temp_part->offset, fw_len,
			ota->data_buf, ota->data_buf_size) != 1) {
			SYS_LOG_ERR("storage is not clean, offs 0x%x size 0x%x", temp_part->offset, fw_len);
			return -EINVAL;
		}

		err = ota_storage_write(ota->storage, temp_part->offset, fw_head, fw_len);
		if (err) {
			SYS_LOG_ERR("storage write failed, offs 0x%x", temp_part->offset);
			return -EIO;
		}
	}

	manifest_data = ota_manifest_get_data(&ota->manifest, &manifest_len);
	if (manifest_data && fw_head) {
		if ((fw_len + manifest_len) >= temp_part->size) {
			SYS_LOG_ERR("temp partition size 0x%x is too small 0x%x", temp_part->size, fw_len + manifest_len);
			return -EINVAL;
		}

		if (ota_storage_is_clean(ota->storage, temp_part->offset + fw_len, manifest_len,
			ota->data_buf, ota->data_buf_size) != 1) {
			SYS_LOG_ERR("storage is not clean, offs 0x%x size 0x%x",
						temp_part->offset + fw_len, manifest_len);
			return -EINVAL;
		}

		err = ota_storage_write(ota->storage, temp_part->offset + fw_len,
								manifest_data, manifest_len);
		if (err) {
			SYS_LOG_ERR("storage write failed, offs 0");
			return -EIO;
		}
	}

	ota->xml_offset = fw_len + manifest_len;
	SYS_LOG_INF("write xml offset 0x%x", ota->xml_offset);

	return 0;
}
#endif

int ota_upgrade_get_temp_img_size(struct ota_upgrade_info *ota)
{
	int file_size;
	struct ota_image *img = ota->img;

	file_size = ota_image_get_file_length(img, NULL);

	if(ota_use_secure_boot(ota)) {
		file_size += OTA_SIGNATURE_DATA_LEN;
	}

	return file_size;
}


static int ota_upgrade_statistics(struct ota_upgrade_info *ota)
{
	const struct partition_entry *part;
	struct ota_manifest *manifest = &ota->manifest;
	struct ota_file *file;
	struct ota_breakpoint *bp = &ota->bp;
	int i, bp_file_state, start_write_offset = 0;
	int cur_file_id;
	uint32_t total_size = 0, ota_size = 0;
	int temp_file_size = 0;

	if (!ota_use_recovery(ota) || ota_use_recovery_app(ota)) {
		for (i = 0; i < manifest->file_cnt; i++) {
			file = &manifest->wfiles[i];
			start_write_offset = 0;
			part = partition_get_mirror_part(file->file_id);
			if (part == NULL) {
				SYS_LOG_INF("no mirror part entry for file_id=%d", file->file_id);

				if (ota_use_recovery_app(ota)) {
					cur_file_id = partition_get_current_file_id();
					part = partition_get_part(file->file_id);
					if (cur_file_id == file->file_id || part == NULL) {
						SYS_LOG_ERR("no part entry for file_id=%d, cur_file_id=%d", file->file_id, cur_file_id);
						continue;
					}
					SYS_LOG_INF("found part entry for file_id=%d, cur_file_id=%d", file->file_id, cur_file_id);
				} else {
					return -EINVAL;
				}
			}

			bp_file_state = ota_breakpoint_get_file_state(bp, file->file_id);
			if (bp_file_state != OTA_BP_FILE_STATE_WRITE_DONE &&
	    		bp_file_state != OTA_BP_FILE_STATE_VERIFY_PASS) {
				if (bp_file_state == OTA_BP_FILE_STATE_WRITING_CLEAN
					|| bp_file_state == OTA_BP_FILE_STATE_WRITING)
					start_write_offset = bp->cur_file_write_offset;
				ota_size += (file->size - start_write_offset);
			}
			total_size += file->size;

			SYS_LOG_INF("%s(%d): fsize=%d, bpoffset=0x%x\n",
				file->name, file->file_id, file->size, start_write_offset);
		}
	} else {
		part = partition_get_temp_part();
		if (part == NULL) {
			SYS_LOG_ERR("cannot found temp partition");
			return -EINVAL;
		}
		bp_file_state = ota_breakpoint_get_file_state(bp, PARTITION_FILE_ID_OTA_TEMP);
		if (bp_file_state != OTA_BP_FILE_STATE_WRITE_DONE &&
    		bp_file_state != OTA_BP_FILE_STATE_VERIFY_PASS) {
			if (bp_file_state == OTA_BP_FILE_STATE_WRITING_CLEAN
				|| bp_file_state == OTA_BP_FILE_STATE_WRITING)
				start_write_offset = bp->cur_file_write_offset;
			temp_file_size = ota_upgrade_get_temp_img_size(ota);
			ota_size += (temp_file_size - start_write_offset);
			total_size += temp_file_size;
		}

		SYS_LOG_INF("temp.bin fsize=%d, bpoffset=0x%x\n", temp_file_size, start_write_offset);

	}

	if (ota_size)
		ota_image_progress_on(ota->img, total_size, ota_size);

	return 0;
}

static void ota_upgrade_dump_time_performance(struct ota_upgrade_info *ota)
{
	ota_storage_time_statistics(ota->storage);
	printk("ota xfer time %d ms\n", ota->xfer_time);
}



int ota_upgrade_check(struct ota_upgrade_info *ota, struct ota_upgrade_check_param *param)
{
	struct ota_breakpoint *bp = &ota->bp;
	struct ota_backend *backend;
	int err, need_upgrade;

	SYS_LOG_INF("start");

	if (ota->state != OTA_INIT) {
		SYS_LOG_WRN("%d, not OTA_INIT state skip upgrade", ota->state);
		//ota_update_state(ota, OTA_CANCEL);
		///return -EINVAL;
	}

	if (ota_upgrade_get_backend_type(ota) == OTA_BACKEND_TYPE_UNKNOWN){
		SYS_LOG_ERR("backend type error\n");
		return -EINVAL;
	}

	if (!param){
		ota->data_buf = mem_malloc(ota->data_buf_size);
		ota->data_buf_owner = true;
	}else{
		ota->data_buf = param->data_buf;
		ota->data_buf_size = param->data_buf_size;
		ota->data_buf_owner = false;
	}

	if (!ota->data_buf) {
		SYS_LOG_ERR("faield to allocate %d bytes", ota->data_buf_size);
		return -ENOMEM;
	}

	ota_breakpoint_init(&ota->bp);

	ota_partition_update_prepare(ota, false);

	ota_update_state(ota, OTA_INIT_FINISHED);

	err = ota_image_open(ota->img);
	if (err) {
		SYS_LOG_INF("ota image open failed");
		ota_update_state(ota, OTA_CANCEL);
		ota_image_close(ota->img);
		ota_breakpoint_exit(&ota->bp);
		if (ota->data_buf && ota->data_buf_owner) {
			mem_free(ota->data_buf);
			ota->data_buf = NULL;
		}
		return -EIO;
	}

	if (ota_use_recovery_app(ota)) {
		/* only check data in recovery app to save time */
		err = ota_image_check_data(ota->img);
		if (err) {
			SYS_LOG_ERR("bad data crc");
			err = -EINVAL;
			goto exit;
		}
	}

	err = ota_manifest_parse_file(&ota->manifest, ota->img, OTA_MANIFESET_FILE_NAME);
	if (err) {
		SYS_LOG_INF("cannot get manifest file in image");
		err = -EAGAIN;
		goto exit;
	}

	/* need upgrade? */
	need_upgrade = ota_is_need_upgrade(ota);
	if (need_upgrade <= 0) {
		SYS_LOG_INF("skip upgrade");
		ota_breakpoint_update_state(bp, OTA_BP_STATE_WRITING_IMG_FAIL);
		err = -EAGAIN;
		goto exit;
	}else if(need_upgrade == 2){
		SYS_LOG_INF("bp changed");
		ota_breakpoint_update_state(bp, OTA_BP_STATE_WRITING_IMG_FAIL);
		ota_partition_update_prepare(ota, false);
	}

	ota_upgrade_statistics(ota);

//Do not do xml writing.
#if 0
	if (ota_use_recovery(ota) && !ota_use_recovery_app(ota)) {
		err = ota_upgrade_write_xml(ota);
		if (err) {
			SYS_LOG_ERR("can not write xml err=%d", err);
			goto exit;
		}
	}
#endif

	ota_image_release_fw_head(ota->img);
	ota_manifest_release_data(&ota->manifest);

	SYS_LOG_INF("to do upgrade");

	backend = ota_image_get_backend(ota->img);
	/* update breakpoint for new firmware */
	bp->backend_type = ota_backend_get_type(backend);
	bp->new_version = ota->manifest.fw_ver.version_code;
	bp->data_checksum = ota_image_get_checksum(ota->img);


	ota->bp_reported = 0;
	ota_update_state(ota, OTA_RUNNING);

	err = ota_rx_init(ota);
	if (err) {
		goto exit;
	}

	if (!ota_use_recovery(ota) || ota_use_recovery_app(ota)) {
		ota_breakpoint_update_state(bp, OTA_BP_STATE_UPGRADE_WRITING);
		err = ota_do_upgrade(ota);
		if (err) {
			SYS_LOG_INF("upgrade failed, err %d", err);
			if (err != -EIO && err != -EAGAIN) {
				ota_breakpoint_update_state(bp, OTA_BP_STATE_UPGRADING_FAIL);
			}
			goto exit;
		}
		ota_breakpoint_update_state(bp, OTA_BP_STATE_UPGRADE_DONE);

		ota_update_state(ota, OTA_UPLOADING);
	}
	else {
		ota_breakpoint_update_state(bp, OTA_BP_STATE_WRITING_IMG);
		err = ota_write_temp_img(ota);
		if (err) {
			SYS_LOG_INF("write ota image failed, err %d", err);
			if (err != -EIO && err != -EAGAIN) {
				ota_breakpoint_update_state(bp, OTA_BP_STATE_WRITING_IMG_FAIL);
			}
			goto exit;
		}

		err = ota_upgrade_verify_temp_image(ota);
		if (err) {
			goto exit;
		}

		err = ota_update_state(ota, OTA_UPGRADE_TEMP_IMG_DONE);
		if(err){
			goto exit;
		}

		err = ota_write_boot_image(ota);
		if (err) {
			SYS_LOG_INF("upgrade failed, err %d", err);
			if (err != -EIO && err != -EAGAIN) {
				ota_breakpoint_update_state(bp, OTA_BP_STATE_UPGRADING_FAIL);
			}
			goto exit;
		}

		err = ota_update_state(ota, OTA_UPLOADING);

		if(err){
			goto exit;
		}

		ota_update_temp_partition_flag(ota);

		ota_breakpoint_update_state(bp, OTA_BP_STATE_UPGRADE_PENDING);

#ifdef CONFIG_OTA_UPGRADE_SAVE_OTA_HEADER
		if(ota_upgrade_get_backend_type(ota) == OTA_BACKEND_TYPE_SELFAPP){
			ota_dfu_breakpoint_clear();
		}
#endif
	}

	SYS_LOG_INF("upgrade successfully!");

	ota_image_report_progress(ota->img, 0, 1);
	ota_image_ioctl(ota->img, OTA_BACKEND_IOCTL_REPORT_IMAGE_VALID, 1);
	ota_update_state(ota, OTA_DONE);

	ota_upgrade_dump_time_performance(ota);

exit:
	ota_breakpoint_exit(&ota->bp);
	ota_rx_exit(ota);

	ota_image_release_fw_head(ota->img);
	ota_manifest_release_data(&ota->manifest);

	if (err) {
		ota_image_ioctl(ota->img, OTA_BACKEND_IOCTL_REPORT_IMAGE_VALID, 0);
		ota_update_state(ota, OTA_FAIL);
		if (err == -EAGAIN) {
			/* wait for upgrade resume */
			SYS_LOG_INF("ota status -> OTA_INIT, wait for resume!");
			ota_update_state(ota, OTA_INIT);
		}
	}

	if (ota->data_buf && ota->data_buf_owner) {
		mem_free(ota->data_buf);
		ota->data_buf = NULL;
	}

	ota_image_close(ota->img);

	return err;
}

int ota_upgrade_attach_backend(struct ota_upgrade_info *ota, struct ota_backend *backend)
{
	struct ota_backend *img_backend = ota_image_get_backend(ota->img);

	SYS_LOG_INF("attach backend type %d", backend->type);

	if (img_backend != NULL && img_backend->type != backend->type) {
		SYS_LOG_ERR("already attached backend %d %d", img_backend->type, backend->type);
		return -EBUSY;
	}

	ota_image_bind(ota->img, backend);

	return 0;
}

void ota_upgrade_detach_backend(struct ota_upgrade_info *ota, struct ota_backend *backend)
{
	struct ota_backend *img_backend = ota_image_get_backend(ota->img);

	SYS_LOG_INF("detach backend %p", backend);

	if (img_backend == backend)
		ota_image_unbind(ota->img, backend);
}

int ota_upgrade_get_backend_type(struct ota_upgrade_info *ota)
{
	struct ota_backend *img_backend = ota_image_get_backend(ota->img);
	if(img_backend){
		return ota_backend_get_type(img_backend);
	}else{
		return OTA_BACKEND_TYPE_UNKNOWN;
	}
}

int ota_upgrade_is_in_progress(struct ota_upgrade_info *ota)
{
	struct ota_breakpoint *bp = &ota->bp;
	int bp_state;

	bp_state = ota_breakpoint_get_current_state(bp);
	switch (bp_state) {
	case OTA_BP_STATE_UPGRADE_PENDING:
	case OTA_BP_STATE_UPGRADE_WRITING:
	case OTA_BP_STATE_UPGRADE_DONE:
		return 1;
	default:
		break;
	}

	return 0;
}

int ota_upgrade_is_ota_running(void)
{
	struct ota_breakpoint bp;
	int bp_state;

	ota_breakpoint_init(&bp);

	bp_state = ota_breakpoint_get_current_state(&bp);
	switch (bp_state) {
	case OTA_BP_STATE_UPGRADE_PENDING:
	case OTA_BP_STATE_UPGRADE_WRITING:
	case OTA_BP_STATE_WRITING_IMG:
	case OTA_BP_STATE_UPGRADE_DONE:
		return 1;
	default:
		break;
	}

	return 0;
}


static struct ota_upgrade_info global_ota_upgrade_info;
struct ota_upgrade_info *ota_upgrade_init(struct ota_upgrade_param *param)
{
	struct ota_upgrade_info *ota;

	SYS_LOG_INF("init");

	ota = &global_ota_upgrade_info;

	memset(ota, 0x0, sizeof(struct ota_upgrade_info));

	if (param->no_version_control) {
		SYS_LOG_INF("enable no version control");
		ota->flags |= OTA_FLAG_USE_NO_VERSION_CONTROL;
	}

	/* allocate data buffer later */
	ota->data_buf_size = OTA_DATA_BUFFER_SIZE;

	ota->storage = ota_storage_init(param->storage_name);
	if (!ota->storage) {
		SYS_LOG_INF("storage open err");
		return NULL;
	}

#ifdef CONFIG_OTA_DOWNLOAD_EXTERNAL_FLASH
	ota->storage_ext = ota_storage_init(param->storage_ext_name);
	if (!ota->storage_ext) {
		SYS_LOG_INF("storage ext open err");
		return NULL;
	}
#endif

	if (param->flag_use_recovery) {
		ota->flags |= OTA_FLAG_USE_RECOVERY;
	}

	if (param->flag_secure_boot) {
		ota->flags |= OTA_FLAG_USE_SECURE_BOOT;
		ota->public_key = param->public_key;
	}

	if (param->flag_use_recovery_app) {
		if (!param->flag_use_recovery) {
			SYS_LOG_ERR("invalid flag_is_recovery_app");
			return NULL;
		}

		ota->flags |= OTA_FLAG_USE_RECOVERY_APP;
	}

	ota_breakpoint_init(&ota->bp);

	if (!param->flag_enable_reboot_resume_upgrade){
		ota_upgrade_clear_ota_bp_state(ota);
		if (!param->flag_skip_init_erase){
			ota_partition_update_prepare_init(ota);
		}
	}else{
		ota_partition_update_prepare(ota, true);
	}


	ota->img = ota_image_init();
	if (!ota->img) {
		SYS_LOG_ERR("image init failed");
		ota_breakpoint_exit(&ota->bp);
		return NULL;
	}

	ota_breakpoint_exit(&ota->bp);

	ota->notify = param->notify;

	ota_update_state(ota, OTA_INIT);

	return ota;
}

void ota_upgrade_exit(struct ota_upgrade_info *ota)
{
	SYS_LOG_INF("exit");

	/* TODO */

	if (ota) {
		if (ota->img)
			ota_image_exit(ota->img);

		if (ota->storage)
			ota_storage_exit(ota->storage);
	}
}

int ota_upgrade_cancel_read(struct ota_upgrade_info *ota)
{
	if (!ota || !ota->img) {
		SYS_LOG_ERR("image init failed");
		return -EIO;
	}

	ota_image_read_cancel(ota->img);
	SYS_LOG_INF("state %d", ota->state);
	if(ota->state == OTA_INIT_FINISHED || ota->state == OTA_INIT) {
		ota_image_close(ota->img);
	}

	return 0;
}


const struct partition_entry *ota_upgrade_get_ota_partition_other_writing(uint8_t file_id)
{
	int i;
	struct ota_breakpoint bp;
	const struct partition_entry *part;
	int file_state;

	ota_breakpoint_init(&bp);

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		part = partition_get_part_by_id(i);
		if (part == NULL)
			break;

		SYS_LOG_INF("part[%d]: bp_state %d, file_id %d", i, bp.state, part->file_id);

		if (part->file_id == 0)
			continue;

#ifndef CONFIG_OTA_RECOVERY
		/* skip current firmware's partitions */
		if (!partition_is_mirror_part(part)) {
			continue;
		}
#endif

		if (part->file_id != file_id){
			continue;
		}


		if (bp.state == OTA_BP_STATE_CLEAN) {
			bp.state = OTA_BP_STATE_UPGRADE_WRITING_OTHER;
			bp.cur_file.offset = part->offset;
			bp.cur_file_write_offset = 0;
			ota_breakpoint_set_file_state(&bp, file_id, OTA_BP_FILE_STATE_OTHER_WRITING);
#ifdef CONFIG_OTA_RECOVERY
			ota_breakpoint_set_file_state(&bp, PARTITION_FILE_ID_OTA_TEMP, OTA_BP_FILE_STATE_OTHER_WRITING);
#else
			ota_breakpoint_set_file_state(&bp, PARTITION_FILE_ID_SYSTEM, OTA_BP_FILE_STATE_OTHER_WRITING);
			ota_breakpoint_set_file_state(&bp, PARTITION_FILE_ID_SDFS, OTA_BP_FILE_STATE_OTHER_WRITING);
#endif
			ota_breakpoint_save(&bp);
			return part;
		}else if(bp.state == OTA_BP_STATE_UPGRADE_WRITING_OTHER){
			/* check breakpoint */
			file_state = ota_breakpoint_get_file_state(&bp, part->file_id);
			if(file_state == OTA_BP_FILE_STATE_OTHER_WRITING){
				return part;
			}
		}
	}

	return NULL;

}

int ota_upgrade_set_ota_partition_other_writing(uint8_t file_id)
{
	int i;
	struct ota_breakpoint bp;
	const struct partition_entry *part;

	ota_breakpoint_init(&bp);

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		part = partition_get_part_by_id(i);
		if (part == NULL)
			break;

		SYS_LOG_INF("part[%d]: bp_state %d, file_id %d", i, bp.state, part->file_id);

		if (part->file_id == 0)
			continue;

#ifndef CONFIG_OTA_RECOVERY
		/* skip current firmware's partitions */
		if (!partition_is_mirror_part(part)) {
			continue;
		}
#endif

		if (part->file_id != file_id){
			continue;
		}

		if(bp.state != OTA_BP_STATE_UPGRADE_WRITING_OTHER){
			bp.state = OTA_BP_STATE_UPGRADE_WRITING_OTHER;
			bp.cur_file.offset = part->offset;
			bp.cur_file_write_offset = 0;
			ota_breakpoint_set_file_state(&bp, file_id, OTA_BP_FILE_STATE_OTHER_WRITING);
#ifdef CONFIG_OTA_RECOVERY
			ota_breakpoint_set_file_state(&bp, PARTITION_FILE_ID_OTA_TEMP, OTA_BP_FILE_STATE_OTHER_WRITING);
#else
			ota_breakpoint_set_file_state(&bp, PARTITION_FILE_ID_SYSTEM, OTA_BP_FILE_STATE_OTHER_WRITING);
			ota_breakpoint_set_file_state(&bp, PARTITION_FILE_ID_SDFS, OTA_BP_FILE_STATE_OTHER_WRITING);
#endif
			ota_breakpoint_save(&bp);

		}

		return 0;
	}

	return -EINVAL;
}

int ota_upgrade_clear_ota_bp_state(struct ota_upgrade_info *ota)
{
	ota_breakpoint_clear_all_file_state(&ota->bp);

	SYS_LOG_INF("ok\n");

	return 0;
}

int ota_upgrade_set_ota_buf(struct ota_upgrade_info *ota, uint8_t *buf, uint32_t buf_size)
{
	if(ota->data_buf){
		return -EINVAL;
	}

	ota->data_buf = buf;
	ota->data_buf_size = buf_size;
	ota->data_buf_owner = false;

	return 0;
}


#ifdef CONFIG_OTA_DOWNLOAD_EXTERNAL_FLASH
int ota_upgrade_flash_code_copy(struct ota_upgrade_info *ota, uint8_t src_partition_id, uint8_t dst_partition_id)
{
	int i;
	const struct partition_entry *src_part;
	const struct partition_entry *dst_part;
	struct ota_storage *src_storage;
	struct ota_storage *dst_storage;
	uint32_t file_offset, file_size;
	uint32_t copy_size, copy_once_size, copy_offset;

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		src_part = partition_get_part_by_id(i);
		if (src_part->file_id == src_partition_id){
			break;
		}
	}

	if(i == MAX_PARTITION_COUNT){
		return -EINVAL;
	}

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		dst_part = partition_get_part_by_id(i);
		if (dst_part->file_id == dst_partition_id){
			break;
		}
	}

	if(i == MAX_PARTITION_COUNT){
		return -EINVAL;
	}

	src_storage = ota_get_part_storage(ota, src_part->storage_id);

	if(src_storage == NULL){
		return -EINVAL;
	}

	dst_storage = ota_get_part_storage(ota, dst_part->storage_id);

	if(dst_storage == NULL){
		return -EINVAL;
	}

	file_offset = ota_image_get_file_offset(ota->img, "TEMP.bin");
	file_size = ota_image_get_file_length(ota->img, "TEMP.bin");

	copy_size = file_offset + file_size;

	copy_size = (copy_size + OTA_ERASE_ALIGN_SIZE -1 ) & ~(OTA_ERASE_ALIGN_SIZE - 1);

	if(copy_size > dst_part->size){
		copy_size = dst_part->size;
	}

	printk("copy code from %x to %x size %x\n", src_part->offset, dst_part->offset, copy_size);

	ota_storage_erase(dst_storage, dst_part->offset, copy_size);

	copy_offset = 0;
	while(copy_size){
		if(copy_size > ota->data_buf_size){
			copy_once_size = ota->data_buf_size;
		}else{
			copy_once_size = copy_size;
		}

		ota_storage_read(src_storage, src_part->offset + copy_offset, ota->data_buf, copy_once_size);

		ota_storage_write(dst_storage, dst_part->offset + copy_offset, ota->data_buf, copy_once_size);

		copy_size -= copy_once_size;
		copy_offset += copy_once_size;
	}

	ota_storage_read(dst_storage, dst_part->offset, ota->data_buf, ota->data_buf_size);

	if(ota->data_buf[0] != 0x41 && ota->data_buf[1] != 0x4F \
		&& ota->data_buf[2] != 0x54 && ota->data_buf[3] != 0x41){

		ota->data_buf[0] = 0x41;
		ota->data_buf[1] = 0x4F;
		ota->data_buf[2] = 0x54;
		ota->data_buf[3] = 0x41;

		ota_storage_write(dst_storage, dst_part->offset, ota->data_buf, 4);
	}
	return 0;
}

#endif





