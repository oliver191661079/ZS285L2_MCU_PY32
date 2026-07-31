/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA disk backend interface
 */

#define SYS_LOG_LEVEL 3
#define SYS_LOG_DOMAIN "otasd"
#include <logging/sys_log.h>

#include <kernel.h>
#include <fs.h>
#include <file_stream.h>
#include <string.h>
#include <mem_manager.h>
#include <fs_manager.h>
#include <ota_backend.h>
#include <ota_backend_disk.h>

#undef CONFIG_OTA_SDCARD_DYNAMIC_DETECT

#if CONFIG_OTA_IMAGE_CHECK_MODE == 1
#define OTA_READ_CACHE_BUF_SIZE 1024
#define BLOCK_SIZE 32
#define FRAME_SIZE (BLOCK_SIZE+2)
#define READ_SIZE ((OTA_READ_CACHE_BUF_SIZE/FRAME_SIZE)*FRAME_SIZE)
#endif

struct ota_backend_disk {
	struct ota_backend backend;
	io_stream_t stream;
	int stream_opened;
	const char *fpath;
#if CONFIG_OTA_IMAGE_CHECK_MODE == 1
	u8_t *cache_buf;
#endif
};

static int disk_file_is_exist(const char* f_path)
{
	struct fs_dirent *entry;
	int err;

	entry = (struct fs_dirent *)mem_malloc(sizeof(struct fs_dirent));
	if (!entry) {
		return 0;
	}

	err = fs_stat(f_path, entry);
	mem_free(entry);
	if (err) {
		SYS_LOG_INF("file %s not found ", f_path);
		return 0;
	}

	SYS_LOG_INF("file %s found", f_path);

	return 1;
}

int ota_backend_disk_ioctl(struct ota_backend *backend, int cmd, unsigned int param)
{
	SYS_LOG_INF("cmd 0x%x: param %d\n", cmd, param);

	switch (cmd) {
	case OTA_BACKEND_IOCTL_REPORT_PROCESS:
		backend->cb(backend, OTA_BACKEND_UPGRADE_PROGRESS, param);
		break;
	default:
		SYS_LOG_ERR("unknow cmd 0x%x", cmd);
		return -EINVAL;
	}

	return 0;
}

#if CONFIG_OTA_IMAGE_CHECK_MODE == 1
int ota_backend_disk_check_read(struct ota_backend_disk *backend_disk, int offset, void *buf, int size)
{
	if(!backend_disk || !buf || !size)
	{
		return -1;
	}

	u32_t block_index = offset/BLOCK_SIZE;
	u32_t block_offset = offset%BLOCK_SIZE;
	u32_t read_offset = (block_index * FRAME_SIZE) + block_offset;
	int err;

	SYS_LOG_INF("read_offset 0x%x, size %d, buf %p", read_offset, size, buf);

	err = stream_seek(backend_disk->stream, read_offset, SEEK_DIR_BEG);
	if (err < 0) {
		SYS_LOG_INF("seek err %d", err);
		return -EIO;
	}

	u8_t * write_addr = (u8_t*)buf;

	if(block_offset){
		err = stream_read(backend_disk->stream, write_addr, block_offset);
		if (err < 0) {
			SYS_LOG_INF("read data err %d", err);
			return -EIO;
		}
		write_addr += block_offset;
		size -= block_offset;
		SYS_LOG_INF("offset tail len %d\n", block_offset);
	}


	while(size)
	{
		err = stream_read(backend_disk->stream, backend_disk->cache_buf, READ_SIZE);
		if (err < 0) {
			SYS_LOG_INF("read cache err %d", err);
			return -EIO;
		}

		u8_t *cache_addr = backend_disk->cache_buf;
		for(u32_t i = 0; i < READ_SIZE; i += FRAME_SIZE)
		{
			if(size >= BLOCK_SIZE)
			{
				memcpy(write_addr, cache_addr, BLOCK_SIZE);
				write_addr += BLOCK_SIZE;
				size -= BLOCK_SIZE;
				cache_addr += FRAME_SIZE;
			}else if (size > 0){
				SYS_LOG_INF("write end %d\n", size);
				memcpy(write_addr, cache_addr, size);
				write_addr += size;
				size -= size;
				break;
			}else if(size < 0){
				SYS_LOG_INF("copy cache err");
				return -1;
			}
		}

		if(size > 0 && size < BLOCK_SIZE)
		{
			SYS_LOG_INF("size tail len %d, %p\n", size, write_addr);
			err = stream_read(backend_disk->stream, write_addr, size);
			if (err < 0) {
				SYS_LOG_INF("read cache err %d", err);
				return -EIO;
			}
			size = 0;
			break;
		}
	}

	return 0;
}
#endif

int ota_backend_disk_read(struct ota_backend *backend, int offset, void *buf, int size)
{
	struct ota_backend_disk *backend_disk = CONTAINER_OF(backend,
		struct ota_backend_disk, backend);

	SYS_LOG_INF("offset 0x%x, size %d, buf %p", offset, size, buf);

#if CONFIG_OTA_IMAGE_CHECK_MODE == 1

	return ota_backend_disk_check_read(backend_disk, offset, buf, size);

#elif CONFIG_OTA_IMAGE_CHECK_MODE == 0
	int err;
	err = stream_seek(backend_disk->stream, offset, SEEK_DIR_BEG);
	if (err < 0) {
		SYS_LOG_INF("seek err %d", err);
		return -EIO;
	}

	err = stream_read(backend_disk->stream, buf, size);
	if (err < 0) {
		SYS_LOG_INF("read data err %d", err);
		return -EIO;
	}
	return 0;
#endif

}

int ota_backend_disk_open(struct ota_backend *backend)
{
	struct ota_backend_disk *backend_disk = CONTAINER_OF(backend,
		struct ota_backend_disk, backend);
	int err;
	if (!fs_manager_get_volume_state("USB:") && !disk_file_is_exist(backend_disk->fpath)) {
		if (!fs_manager_get_volume_state("SD:") && !disk_file_is_exist(backend_disk->fpath)) {
			SYS_LOG_ERR("cannot found ota file \'%s\' in disk", backend_disk->fpath);
			return -ENOENT;
		}
	}

#if CONFIG_OTA_IMAGE_CHECK_MODE == 1
	backend_disk->cache_buf =  mem_malloc(OTA_READ_CACHE_BUF_SIZE);
	if(!backend_disk->cache_buf)
	{
		SYS_LOG_ERR("malloc cache_buf failed\n");
		return -ENOMEM;
	}

	SYS_LOG_INF("malloc cache_buf \n");

#endif

	backend_disk->stream = file_stream_create((char *)backend_disk->fpath);
	if (!backend_disk->stream) {
		SYS_LOG_ERR("stream create failed \n");
		return -EIO;
	}

	SYS_LOG_INF("create stream %p", backend_disk->stream);
	err = stream_open(backend_disk->stream, MODE_IN);
	if (err) {
		SYS_LOG_ERR("stream open failed \n");
		return err;
	}

	backend_disk->stream_opened = 1;

	SYS_LOG_INF("open stream %p", backend_disk->stream);


	return 0;
}

int ota_backend_disk_close(struct ota_backend *backend)
{
	struct ota_backend_disk *backend_disk = CONTAINER_OF(backend,
		struct ota_backend_disk, backend);
	int err;

	SYS_LOG_INF("close: type %d", backend->type);

	if (backend_disk->stream) {
		err = stream_close(backend_disk->stream);
		if (err) {
			SYS_LOG_ERR("stream_close Failed");
			return err;
		}
	}

	backend_disk->stream_opened = 0;

#if CONFIG_OTA_IMAGE_CHECK_MODE == 1
	if(backend_disk->cache_buf)
	{
		mem_free(backend_disk->cache_buf);
		backend_disk->cache_buf = NULL;
		SYS_LOG_INF("free cache_buf \n");
	}
#endif

	return 0;
}

void ota_backend_disk_exit(struct ota_backend *backend)
{
	struct ota_backend_disk *backend_disk = CONTAINER_OF(backend,
		struct ota_backend_disk, backend);

	if (backend_disk->stream) {
		stream_destroy(backend_disk->stream);
	}

	mem_free(backend_disk);
}

const struct ota_backend_api ota_backend_api_disk = {
	.init = (void *)ota_backend_disk_init,
	.exit = ota_backend_disk_exit,
	.open = ota_backend_disk_open,
	.close = ota_backend_disk_close,
	.read = ota_backend_disk_read,
	.ioctl = ota_backend_disk_ioctl,
};

struct ota_backend *ota_backend_disk_init(ota_backend_notify_cb_t cb,
		struct ota_backend_disk_init_param *param)
{
	struct ota_backend_disk *backend_disk;

	SYS_LOG_INF("init %s",param->fpath);

	backend_disk = mem_malloc(sizeof(struct ota_backend_disk));
	if (!backend_disk) {
		SYS_LOG_ERR("malloc failed");
		return NULL;
	}

	memset(backend_disk, 0x0, sizeof(struct ota_backend_disk));

	backend_disk->fpath = param->fpath;

	ota_backend_init(&backend_disk->backend, OTA_BACKEND_TYPE_CARD,
			(struct ota_backend_api *) &ota_backend_api_disk, cb);

	if ((fs_manager_get_volume_state("SD:") && disk_file_is_exist(backend_disk->fpath))
		 || (fs_manager_get_volume_state("USB:") && disk_file_is_exist(backend_disk->fpath))) {
		SYS_LOG_INF("found ota file \'%s\' in disk", backend_disk->fpath);
		cb(&backend_disk->backend, OTA_BACKEND_UPGRADE_STATE, 1);
	}
#ifndef CONFIG_OTA_SDCARD_DYNAMIC_DETECT
	else {
		mem_free(&backend_disk->backend);
		return NULL;
	}
#endif

	return &backend_disk->backend;
}
