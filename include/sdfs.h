/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief sdfs interface
 */

#ifndef __SDFS_H__
#define __SDFS_H__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef CONFIG_MEMORY
#include <mem_manager.h>
#endif
#ifndef FS_SEEK_SET
#define FS_SEEK_SET	0	/* Seek from beginning of file. */
#endif
#ifndef FS_SEEK_CUR
#define FS_SEEK_CUR	1	/* Seek from current position. */
#endif
#ifndef FS_SEEK_END
#define FS_SEEK_END	2	/* Seek from end of file.  */
#endif

struct sd_file
{
	int start;
	int size;
	int readptr;
};

struct sd_dir
{
	const unsigned char fname[12];	//8+1+3
	int offset;
	int size;
	unsigned int reserved[2];
	unsigned int checksum;
};
#ifdef CONFIG_MEMORY
#define sd_alloc mem_malloc
#define sd_free mem_free
#else
#define sd_alloc(x) NULL 
#define sd_free(x)
#endif

#define memcpy_flash_data memcpy

struct sd_file * sd_fopen (const char *filename);
void sd_fclose(struct sd_file *sd_file);
int sd_fread(struct sd_file *sd_file, void *buffer, int len);
int sd_ftell(struct sd_file *sd_file);
int sd_fseek(struct sd_file *sd_file, int offset, unsigned char whence);
int sd_fsize(const char *filename);
int sd_fmap(const char *filename, void** addr, int *len);
#endif