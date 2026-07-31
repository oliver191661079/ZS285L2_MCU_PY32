#include "spinor_logical_inner.h"

void __spinor_logical_read_one_sector(unsigned int lba)
{
#ifdef CONFIG_ESD_EN
	int i;
#endif
	struct spinor_logical_sector *tmp = spinor_logical->tmp;

	spinor_read(lba, (void*)tmp, 1, 1);

#ifdef CONFIG_ESD_EN
	for(i = 0; i < CONFIG_ESD_RETRY_COUNT; i++)
	{
		if(tmp->checksum == __spinor_logical_sector_checksum())
			return;
		spinor_read(lba, (void*)tmp, 1, 0);
		if(!__spinor_buf_is_not_all_ff((void*)tmp, 512 / 8))
			return;
    	spinor_read(lba, (void*)tmp, 1, 1);
	}
    /*跑到这里，说明上次program只进行了一半，里面的数据无效*/
	memset(tmp, 0xbd, 512);	//0xbd > sector(max is 128)
	tmp->checksum = __spinor_logical_sector_checksum();
#endif
}

void __spinor_logical_read_sector_in_datablock(unsigned char logical_blk, unsigned char sector)
{
	short data_block_lba;
	struct spinor_logical_sector *tmp = spinor_logical->tmp;

	data_block_lba = spinor_logical->logical_table[logical_blk].phyblk * SPINOR_BLOCK_SECTORS;
	if(data_block_lba == 0)
	{
		memset(tmp->info.data, 0xab, DATA_SIZE_PER_LOGICAL_SECTOR);
		tmp->info.logical_blk = logical_blk;
		tmp->info.blkseq = 0;
		tmp->info.sector = sector;
		return;
	}

	__spinor_logical_read_one_sector(data_block_lba + sector);
	if(tmp->info.sector != sector)
		__spinor_logical_read_one_sector(data_block_lba + tmp->info.sector);
}


void __spinor_logical_read_sector_from_log_block_or_data_block(unsigned char logical_blk, unsigned char sector)
{
	unsigned char i, phy_sector;
	struct spinor_block_info *log_info = NULL, *log_save;

	for(i = 0; i < SPINOR_MAX_LOG_NUM; i++)
	{
		if(spinor_logical->log_table[i].phyblk != BLOCK_NULL
			&& spinor_logical->log_table[i].logical_blk == logical_blk)
		{
			log_info = &spinor_logical->log_table[i];
			break;
		}
	}

	if(log_info != NULL)
	{
		log_save = spinor_logical->log_info;
		spinor_logical->log_info = log_info;

		phy_sector = __spinor_logical_get_log_block_phy_sector(sector);
		if(__log_block_get_sector_status(phy_sector))
		{
			__spinor_logical_read_one_sector((spinor_logical->log_info->phyblk * SPINOR_BLOCK_SECTORS) + phy_sector);
			spinor_logical->log_info = log_save;
			return;
		}
		spinor_logical->log_info = log_save;
	}
	__spinor_logical_read_sector_in_datablock(logical_blk, sector);
}

int __spinor_logical_read(unsigned int logical_address, char *buf, unsigned int size)
{
	unsigned char logical_blk, sector;
	short start_in_sector, size_in_sector;
	struct spinor_logical_sector *tmp;

	if(logical_address + size > spinor_logical->logical_size)
		return -ESPIPE;

	tmp = spinor_logical->tmp;

	while(size > 0)
	{
		logical_blk = logical_address / DATA_SIZE_PER_LOGICAL_BLOCK;
		sector = (logical_address % DATA_SIZE_PER_LOGICAL_BLOCK) / DATA_SIZE_PER_LOGICAL_SECTOR;
		start_in_sector = logical_address % DATA_SIZE_PER_LOGICAL_SECTOR;
		size_in_sector = DATA_SIZE_PER_LOGICAL_SECTOR - start_in_sector;
		size_in_sector = size_in_sector > size ? size : size_in_sector;

		__spinor_logical_read_sector_from_log_block_or_data_block(logical_blk, sector);
		memcpy(buf, tmp->info.data + start_in_sector, size_in_sector);

		logical_address += size_in_sector;
		buf += size_in_sector;
		size -= size_in_sector;
	}
	return 0;
}

static int __spinor_logical_read_from_cache(unsigned int logical_address, char *buf, unsigned int size)
{
	unsigned int cached_address;
	struct spinor_logical_sector *tmp = spinor_logical->tmp;

	if(spinor_logical->log_info->last_write_sector == -1)
		return -1;

	cached_address = (tmp->info.logical_blk * DATA_SIZE_PER_LOGICAL_BLOCK) + (spinor_logical->log_info->last_write_sector * DATA_SIZE_PER_LOGICAL_SECTOR);
	if(logical_address < cached_address || logical_address + size > cached_address + DATA_SIZE_PER_LOGICAL_SECTOR)
		return -1;

	memcpy(buf, tmp->info.data + (logical_address - cached_address), size);

	return 0;
}

static void spinor_logical_physical_read_unaligned(unsigned int phy_addr, int size, void *buf, void *tmp_512_buf)
{
	unsigned int lba, remainder, sectors, remainder_size;

	remainder = phy_addr % 512;
	lba = phy_addr / 512;

	if(remainder != 0)
	{
		remainder_size = 512 - remainder;
		remainder_size = remainder_size > size ? size : remainder_size;

		spinor_read(lba, tmp_512_buf, 1, 0);
		memcpy(buf, tmp_512_buf + remainder, remainder_size);

		buf += remainder_size;
		lba++;
		size -= remainder_size;
		if(size <= 0)
			return;
	}

	remainder_size = size % 512;
	sectors = size / 512;

	if(remainder_size)
	{
		spinor_read(lba + sectors, tmp_512_buf, 1, 0);
		memcpy(buf + (sectors * 512), tmp_512_buf, remainder_size);
	}

	while(sectors-- > 0)
	{
		spinor_read(lba, buf, 1, 0);

		lba++;
		buf += 512;
		size -= 512;
	}
}

int spinor_logical_read(unsigned int logical_address, char *buf, unsigned int size, int mode)
{
	int ret;

	if(size == 0)
		return 0;

	sem_wait(&spinor_logical_semphore);


	if(mode == 0)
	{
		ret = __spinor_logical_read_from_cache(logical_address, buf, size);
		if(ret < 0)
		{
			__spinor_logical_flush();
			ret = __spinor_logical_read(logical_address, buf, size);
		}
	}
	else
	{
		if((logical_address / SPINOR_BLOCK_SIZE) < spinor_logical->physical_phyblk
			|| ((logical_address + size + SPINOR_BLOCK_SIZE - 1) / SPINOR_BLOCK_SIZE) > spinor_logical->physical_phyblk + spinor_logical->physical_blknum)
		{
			ret = -EPERM;
		}
		else
		{
			if((size & 0x1ff) || (logical_address & 0x1ff))
			{
				__spinor_logical_flush();
				spinor_logical_physical_read_unaligned(logical_address, size, buf, spinor_logical->tmp);
			}
			else
			{
				spinor_read(logical_address >> 9, buf, size >> 9, 0);
			}
			ret = 0;
		}
	}
	sem_post(&spinor_logical_semphore);

	return ret;
}


