/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/types.h>
#include <errno.h>
#include <init.h>
#include <ff.h>
#include <fs.h>
#include <mem_manager.h>
#include <misc/__assert.h>

#if 0
static FATFS fat_fs;	/* FatFs work area */
#endif

static int translate_error(int error)
{
	switch (error) {
	case FR_OK:
		return 0;
	case FR_NO_FILE:
	case FR_NO_PATH:
	case FR_INVALID_NAME:
		return -ENOENT;
	case FR_DENIED:
		return -EACCES;
	case FR_EXIST:
		return -EEXIST;
	case FR_INVALID_OBJECT:
		return -EBADF;
	case FR_WRITE_PROTECTED:
		return -EROFS;
	case FR_INVALID_DRIVE:
	case FR_NOT_ENABLED:
	case FR_NO_FILESYSTEM:
		return -ENODEV;
	case FR_NOT_ENOUGH_CORE:
		return -ENOMEM;
	case FR_TOO_MANY_OPEN_FILES:
		return -EMFILE;
	case FR_INVALID_PARAMETER:
		return -EINVAL;
	case FR_LOCKED:
	case FR_TIMEOUT:
	case FR_MKFS_ABORTED:
	case FR_DISK_ERR:
	case FR_INT_ERR:
	case FR_NOT_READY:
		return -EIO;
	}

	return -EIO;
}

int fs_open_cluster(fs_file_t *zfp, char *dir, u32_t cluster, u32_t blk_ofs)
{
	FRESULT res;
	u8_t fs_mode;

	fs_mode = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;

	res = f_open_cluster(&zfp->fp, dir, cluster, blk_ofs, fs_mode);

	if(res){
		//cluster open for lcmusic open read-only file
		printk("open rw failed, try open by read-only\n");
		fs_mode = FA_READ | FA_OPEN_ALWAYS;
		res = f_open_cluster(&zfp->fp, dir, cluster, blk_ofs, fs_mode);
	}

	return translate_error(res);
}

int fs_open(fs_file_t *zfp, const char *file_name)
{
	FRESULT res;
	u8_t fs_mode;

	fs_mode = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;

	res = f_open(&zfp->fp, file_name, fs_mode);

	return translate_error(res);
}

int fs_close(fs_file_t *zfp)
{
	FRESULT res;

	res = f_close(&zfp->fp);

	return translate_error(res);
}

int fs_unlink(const char *path)
{
	FRESULT res;

	res = f_unlink(path);

	return translate_error(res);
}

ssize_t fs_read(fs_file_t *zfp, void *ptr, size_t size)
{
	FRESULT res;
	unsigned int br;

	res = f_read(&zfp->fp, ptr, size, &br);
	if (res != FR_OK) {
		return translate_error(res);
	}

	return br;
}

ssize_t fs_write(fs_file_t *zfp, const void *ptr, size_t size)
{
	FRESULT res;
	unsigned int bw;

	res = f_write(&zfp->fp, ptr, size, &bw);
	if (res != FR_OK) {
		return translate_error(res);
	}

	return bw;
}

int fs_seek(fs_file_t *zfp, off_t offset, int whence)
{
	FRESULT res = FR_OK;
	off_t pos;

	switch (whence) {
	case FS_SEEK_SET:
		pos = offset;
		break;
	case FS_SEEK_CUR:
		pos = f_tell(&zfp->fp) + offset;
		break;
	case FS_SEEK_END:
		pos = f_size(&zfp->fp) + offset;
		break;
	default:
		return -EINVAL;
	}

	if ((pos < 0) || (pos > f_size(&zfp->fp))) {
		return -EINVAL;
	}

	res = f_lseek(&zfp->fp, pos);

	return translate_error(res);
}

int fs_map(const char *file_name, void** addr)
{
	fs_file_t zfp;
	FRESULT res = FR_OK;
	u8_t fs_mode;

	fs_mode = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;

	res = f_open(&zfp.fp, file_name, fs_mode);
	if (res != FR_OK){
		goto exit;
	}

	res = f_map(&zfp.fp, addr);
	if (res != FR_OK){
		goto exit1;
	}

exit1:
	res = f_close(&zfp.fp);
	if (res != FR_OK){
		goto exit;
	}
exit:
	return translate_error(res);
}

int fs_truncate(fs_file_t *zfp, off_t length)
{
	FRESULT res = FR_OK;
	off_t cur_length = f_size(&zfp->fp);

	/* f_lseek expands file if new position is larger than file size */
	res = f_lseek(&zfp->fp, length);
	if (res != FR_OK) {
		return translate_error(res);
	}

	if (length < cur_length) {
		res = f_truncate(&zfp->fp);
	} else {
		/*
		 * Get actual length after expansion. This could be
		 * less if there was not enough space in the volume
		 * to expand to the requested length
		 */
		length = f_tell(&zfp->fp);

		res = f_lseek(&zfp->fp, cur_length);
		if (res != FR_OK) {
			return translate_error(res);
		}

		/*
		 * The FS module does caching and optimization of
		 * writes. Here we write 1 byte at a time to avoid
		 * using additional code and memory for doing any
		 * optimization.
		 */
		unsigned int bw;
		u8_t c = 0;

		for (int i = cur_length; i < length; i++) {
			res = f_write(&zfp->fp, &c, 1, &bw);
			if (res != FR_OK) {
				break;
			}
		}
	}

	return translate_error(res);
}

int fs_trunc_quick(fs_file_t *zfp, off_t length)
{
	FRESULT res = FR_OK;
	off_t cur_length = f_size(&zfp->fp);

	/* Optimize: No need to do any operation */
	if (cur_length == length)
		return translate_error(res);

	/* f_lseek expands file if new position is larger than file size */
	res = f_lseek(&zfp->fp, length);
	if (res != FR_OK) {
		return translate_error(res);
	}

	if (length < cur_length) {
		res = f_truncate(&zfp->fp);
	} else {
		/*
		 * Optimize: Do sync instead of filling the whole
		 * expanded region with zeroes.
		 */
		 res = f_sync(&zfp->fp);
	}

	return translate_error(res);
}

int fs_sync(fs_file_t *zfp)
{
	FRESULT res = FR_OK;

	res = f_sync(&zfp->fp);

	return translate_error(res);
}

int fs_mkdir(const char *path)
{
	FRESULT res;

	res = f_mkdir(path);

	return translate_error(res);
}
int fs_rename(const char *path_old, const char *path_new)
{
	FRESULT res;

	res = f_rename(path_old, path_new);

	return translate_error(res);
}

int fs_opendir_cluster(fs_dir_t *zdp, const char *path, unsigned int cluster, unsigned int blk_ofs)
{
	FRESULT res;

	res = f_opendir_cluster(&zdp->dp, path, cluster, blk_ofs);

	return translate_error(res);
}
int fs_opendir(fs_dir_t *zdp, const char *path)
{
	FRESULT res;

	res = f_opendir(&zdp->dp, path);

	return translate_error(res);
}

int fs_opendir_ofs(fs_dir_t *zdp, const char *path)
{
	FRESULT res;

	res = f_opendir_ofs(&zdp->dp, path);

	return translate_error(res);
}

int fs_readdir(fs_dir_t *zdp, struct fs_dirent *entry)
{
	FRESULT res;
#ifdef CONFIG_MMC
	FILINFO *fno = mem_malloc(sizeof(FILINFO));
#else
	__fs_bss static u8_t buf[sizeof(FILINFO)];
	FILINFO *fno = (FILINFO *)buf;
#endif
	if (!fno)
		return -ENOMEM;

	res = f_readdir(&zdp->dp, fno);
	if (res == FR_OK) {
		entry->type = ((fno->fattrib & AM_DIR) ?
			       FS_DIR_ENTRY_DIR : FS_DIR_ENTRY_FILE);
		strcpy(entry->name, fno->fname);
		entry->size = fno->fsize;
	}

#ifdef CONFIG_MMC
	mem_free(fno);
#endif

	return translate_error(res);
}

int fs_closedir(fs_dir_t *zdp)
{
	FRESULT res;

	res = f_closedir(&zdp->dp);

	return translate_error(res);
}

int fs_stat(const char *path, struct fs_dirent *entry)
{
	FRESULT res;
	FILINFO *fno = mem_malloc(sizeof(FILINFO));
	if (!fno)
		return -ENOMEM;

	res = f_stat(path, fno);
	if (res == FR_OK) {
		entry->type = ((fno->fattrib & AM_DIR) ?
			       FS_DIR_ENTRY_DIR : FS_DIR_ENTRY_FILE);
		strcpy(entry->name, fno->fname);
		entry->size = fno->fsize;
	}

	mem_free(fno);

	return translate_error(res);
}

int fs_statvfs(const char *vol_name, struct fs_statvfs *stat)
{
	FATFS *fs;
	FRESULT res;

	res = f_getfree(vol_name, &stat->f_bfree, &fs);
	if (res != FR_OK) {
		return -EIO;
	}

	/*
	 * _MIN_SS holds the sector size. It is one of the configuration
	 * constants used by the FS module
	 */
	stat->f_bsize = _MIN_SS;
	stat->f_frsize = fs->csize * stat->f_bsize;
	stat->f_blocks = (fs->n_fatent - 2);

	return translate_error(res);
}

/* detect disk add or remove */
int fs_disk_detect(const char *str, u8_t *state)
{
	return f_disk_detect(str, state);
}

/* mount disk */
int fs_disk_init(FATFS* fs, const char *str)
{
	FRESULT res;

	res = f_mount(fs, str, 1);

#if 0
	/* If no file system found, do nothing in case of stack overflow */
	__ASSERT((res == FR_OK), "FS (%s) init failed (%d)",
				str, translate_error(res));
#endif

	return res;
}

/* unmount disk */
int fs_disk_uninit(const char *str)
{
	FRESULT res;

	res = f_mount(NULL, str, 1);

	__ASSERT((res == FR_OK), "FS (%s) uninit failed (%d)",
			str, translate_error(res));

	return res;
}
/* fs init by app system */
#if 0
static int fs_init(struct device *dev)
{
	FRESULT res;
	int i = 0;
	char volume_path[16];

	ARG_UNUSED(dev);

	static const char* const volume_str[] = {_VOLUME_STRS};

	memset(volume_path, 0, sizeof(volume_path));

	for(i = 0; i < _VOLUMES; i++)
	{
		sprintf(volume_path,"%s:",volume_str[i]);
		res = f_mount(&fat_fs[i], volume_path, 1);
		/* If no file system found then create one */
		if (res == FR_NO_FILESYSTEM) {
			uint8_t work[_MAX_SS];

			res = f_mkfs(volume_path, (FM_FAT | FM_SFD), 0, work, sizeof(work));
			if (res == FR_OK) {
				res = f_mount(&fat_fs[i], volume_path, 1);
			}
		}

		__ASSERT((res == FR_OK), "FS init failed (%d)", translate_error(res));
	}
	return translate_error(res);
}

SYS_INIT(fs_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif
