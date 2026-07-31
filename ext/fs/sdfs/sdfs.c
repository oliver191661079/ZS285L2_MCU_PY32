#include <init.h>
#include <kernel.h>
#include <kernel_structs.h>
#include <sdfs.h>
#include <strings.h>
#include <partition.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>


static struct sd_dir * sd_find_dir(const char *filename, void *buf_size_32)
{
	int num, total, offset;
	struct sd_dir *sd_dir = buf_size_32;

	memcpy_flash_data(buf_size_32, (void *)CONFIG_SD_FS_VADDR_START, sizeof(*sd_dir));

	//printk("sd_dir->fname %s CONFIG_SD_FS_START 0x%x \n",sd_dir->fname,CONFIG_SD_FS_VADDR_START);
	if(memcmp(sd_dir->fname, "sdfs.bin", 8) != 0)
	{
		return NULL;
	}
	total = sd_dir->offset;

	for(offset = CONFIG_SD_FS_VADDR_START + sizeof(*sd_dir), num = 0; num < total; offset += 32)
	{
		memcpy_flash_data(buf_size_32, (void *)offset, 32);

		if(strncasecmp(filename, sd_dir->fname, 12) == 0)
		{
			return sd_dir;
		}
		num++;
	}

	return NULL;
}

struct sd_file * sd_fopen (const char *filename)
{
	struct sd_dir *sd_dir;
	u8_t buf_size_32[32];
	struct sd_file *sd_file;

	sd_dir = sd_find_dir(filename, (void *)buf_size_32);
	if(sd_dir == NULL)
	{
		//printk("%s no this file %s\n", __FUNCTION__, filename);
		return NULL;
	}

	sd_file = sd_alloc(sizeof(*sd_file));
	if(sd_file == NULL)
	{
		printk("%s malloc(%d) failed\n", __FUNCTION__, (int)sizeof(*sd_file));
		return NULL;
	}

	sd_file->start = sd_dir->offset + CONFIG_SD_FS_VADDR_START;
	sd_file->size = sd_dir->size;
	sd_file->readptr = sd_file->start;

	return sd_file;
}

void sd_fclose(struct sd_file *sd_file)
{
	sd_free(sd_file);
}

int sd_fread(struct sd_file *sd_file, void *buffer, int len)
{
	unsigned int size_in_512, read_size;

	if ((sd_file->readptr - sd_file->start + len) > sd_file->size)
	{
		len = sd_file->size - (sd_file->readptr - sd_file->start);
	}
	if(len <= 0)
		return 0;

	read_size = len;

	size_in_512 = 512 - (sd_file->readptr % 512);
	size_in_512 = size_in_512 > len ? len : size_in_512;
	if(size_in_512 > 0)
	{
		memcpy_flash_data(buffer, (void *)sd_file->readptr, size_in_512);

		buffer += size_in_512;
		sd_file->readptr += size_in_512;
		len -= size_in_512;
	}

	for(; len > 0; buffer += size_in_512, sd_file->readptr += size_in_512, len -= size_in_512)
	{
		size_in_512 = len > 512 ? 512 : len;

		memcpy_flash_data(buffer, (void *)sd_file->readptr, size_in_512);
	}

	return read_size;
}

int sd_ftell(struct sd_file *sd_file)
{
    return (sd_file->readptr - sd_file->start);
}

int sd_fseek(struct sd_file *sd_file, int offset, unsigned char whence)
{
	if (whence == FS_SEEK_SET)
	{
		if (offset > sd_file->size)
			return -1;

		sd_file->readptr = sd_file->start + offset;
		return 0;
	}

	if (whence == FS_SEEK_CUR)
	{
		if(sd_file->readptr + offset < sd_file->start
			|| sd_file->readptr + offset > sd_file->start + sd_file->size)
		{
			return -1;
		}

		sd_file->readptr += offset;
		return 0;
	}

	if (whence == FS_SEEK_END)
	{
		if(offset > 0 || offset + sd_file->size < 0)
			return -1;

		sd_file->readptr = sd_file->start + sd_file->size + offset;
		return 0;
	}

	return -EINVAL;
}

int sd_fsize(const char *filename)
{
	struct sd_file *fd = sd_fopen(filename);
	int file_size;

	if (!fd) {
		return -EINVAL;
	}

	file_size = fd->size;

	sd_fclose(fd);

	return file_size;
}

int sd_fmap(const char *filename, void** addr, int* len)
{
	struct sd_file *fd = sd_fopen(filename);
	if (!fd) {
		return -EINVAL;
	}

	if (addr)
		*addr = (void *)fd->start;

	if (len)
		*len = fd->size;

	sd_fclose(fd);

	return 0;
}

static int sd_fs_init(struct device *dev)
{
	int err;

	printk("sdfs: init mapping to 0x%x\n", CONFIG_SD_FS_VADDR_START);

	err = partition_file_mapping(PARTITION_FILE_ID_SDFS, CONFIG_SD_FS_VADDR_START);
	if (err) {
		printk("sdfs: cannot mapping part file_id %d", PARTITION_FILE_ID_SDFS);
		return -1;
	}

	return 0;
}

SYS_INIT(sd_fs_init, PRE_KERNEL_1, 80);
