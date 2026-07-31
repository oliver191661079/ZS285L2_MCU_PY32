#include "spinor_logical_inner.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SPINOR_LOGICAL_WRITE

static void __spinor_replace_datablock_with_log_block(unsigned char need_recycle)
{
	unsigned char logical_blk, phyblk;

	logical_blk = spinor_logical->log_info->logical_blk;
	phyblk = spinor_logical->logical_table[logical_blk].phyblk;
	if(phyblk != BLOCK_NULL)
		__spinor_logical_put_free_phy_block(phyblk, need_recycle);

	spinor_logical->logical_table[logical_blk].phyblk = spinor_logical->log_info->phyblk;
	spinor_logical->logical_table[logical_blk].blkseq = spinor_logical->log_info->blkseq;
	spinor_logical->log_info->phyblk = BLOCK_NULL;
}

static void __spinor_logical_program_one_sector(unsigned int lba)
{
#ifdef CONFIG_ESD_EN
	int i;
#endif
	struct spinor_logical_sector *tmp = spinor_logical->tmp;

	spinor_basic_program_sectors(lba, (void*)tmp, 1, 1);

#ifdef CONFIG_ESD_EN
	for(i = 0; i < CONFIG_ESD_RETRY_COUNT; i++)
	{
    	spinor_read(lba, (void*)tmp, 1, 1);
    	if(tmp->checksum == __spinor_logical_sector_checksum())
            return;
    }
	except_reboot();
#endif
}


void __spinor_logical_merge_log_block_and_replace_datablock(unsigned char need_recycle)
{
	unsigned char sector, data_block_sector_1st, phy_sector;
	short log_block_lba, data_block_lba;
	struct spinor_logical_sector *tmp;

	if(spinor_logical->log_info->phyblk == BLOCK_NULL)
		return;

	__spinor_logical_flush();

	tmp = spinor_logical->tmp;
	log_block_lba = spinor_logical->log_info->phyblk * SPINOR_BLOCK_SECTORS;

	data_block_lba = spinor_logical->logical_table[spinor_logical->log_info->logical_blk].phyblk * SPINOR_BLOCK_SECTORS;
	if(data_block_lba == 0)
	{
		data_block_sector_1st = 0;
	}
	else
	{
		__spinor_logical_read_one_sector(data_block_lba);
		data_block_sector_1st = tmp->info.sector;
	}

	for(sector = 0; sector < SPINOR_BLOCK_SECTORS; sector++)
	{
		phy_sector = __spinor_logical_get_log_block_phy_sector(sector);
		if(__log_block_get_sector_status(phy_sector))
			continue;

		if(data_block_lba != 0)
		{
			if(sector == data_block_sector_1st)
				__spinor_logical_read_one_sector(data_block_lba + 0);
			else if(sector == 0)
				__spinor_logical_read_one_sector(data_block_lba + data_block_sector_1st);
			else
				__spinor_logical_read_one_sector(data_block_lba + sector);
		}
		else
		{
			memset(tmp, 0xbc, 512);	//0xbc > sector(max is 128)
		}

		if(tmp->info.sector != sector || phy_sector == 0)
		{
			tmp->info.logical_blk = spinor_logical->log_info->logical_blk;
			tmp->info.blkseq = spinor_logical->log_info->blkseq;
			tmp->info.sector = sector;
			tmp->extend = -1;
			tmp->checksum = __spinor_logical_sector_checksum();
		}
		__spinor_logical_program_one_sector(log_block_lba + phy_sector);
	}

	__spinor_replace_datablock_with_log_block(need_recycle);
}

static void __spinor_logical_check_and_update_log_block(unsigned char logical_blk, unsigned char sector)
{
	if(spinor_logical->log_info->phyblk != BLOCK_NULL)
	{
		if(logical_blk != spinor_logical->log_info->logical_blk
			|| __log_block_get_sector_status(__spinor_logical_get_log_block_phy_sector(sector)))
		{
			__spinor_logical_merge_log_block_and_replace_datablock(1);
			__spinor_logical_new_log_block(logical_blk);
		}
	}
	else
	{
		__spinor_logical_new_log_block(logical_blk);
	}
}

static void __spinor_logical_write_sector_to_log_block(unsigned char sector, unsigned short extend)
{
	unsigned char phy_sector;
	struct spinor_logical_sector *tmp = spinor_logical->tmp;

	/*如果第一个sector未标记，则先写第一个sector*/
	if(__log_block_get_sector_status(0) == 0)
		spinor_logical->log_info->sector_1st = sector;

	phy_sector = __spinor_logical_get_log_block_phy_sector(sector);
	__log_block_set_sector_status(phy_sector);

	tmp->info.logical_blk = spinor_logical->log_info->logical_blk;
	tmp->info.blkseq = spinor_logical->log_info->blkseq;
	tmp->info.sector = sector;
	tmp->extend = extend;
	tmp->checksum = __spinor_logical_sector_checksum();
	__spinor_logical_program_one_sector((spinor_logical->log_info->phyblk * SPINOR_BLOCK_SECTORS) + phy_sector);
}

void __spinor_logical_flush(void)
{
	if(spinor_logical->log_info->last_write_sector != -1)
	{
		__spinor_logical_write_sector_to_log_block(spinor_logical->log_info->last_write_sector, -1);
		spinor_logical->log_info->last_write_sector = -1;
	}
}

static void __spinor_logical_switch_log_block(unsigned char logical_blk)
{
	int i;

	if(spinor_logical->log_info->phyblk != BLOCK_NULL
		&& spinor_logical->log_info->logical_blk == logical_blk)
		return;

	/*切换日志，先将缓存写回*/
	__spinor_logical_flush();

	/*先找完全匹配项*/
	for(i = 0; i < SPINOR_MAX_LOG_NUM; i++)
	{
		if(spinor_logical->log_table[i].phyblk != BLOCK_NULL
			&& spinor_logical->log_table[i].logical_blk == logical_blk)
		{
			spinor_logical->log_info = &spinor_logical->log_table[i];
			return;
		}
	}

	/*再找空项*/
	for(i = 0; i < SPINOR_MAX_LOG_NUM; i++)
	{
		if(spinor_logical->log_table[i].phyblk == BLOCK_NULL)
		{
			spinor_logical->log_info = &spinor_logical->log_table[i];
			return;
		}
	}

	/*最后再找连续项*/
	for(i = 0; i < SPINOR_MAX_LOG_NUM; i++)
	{
		if(spinor_logical->log_table[i].logical_blk + 1 == logical_blk)
		{
			spinor_logical->log_info = &spinor_logical->log_table[i];
			return;
		}
	}
}

int spinor_logical_move(unsigned int dst_logical_address, unsigned int src_logical_address, int size, int mode)
{
	uint8 dst_logical_blk, dst_sector, src_logical_blk, src_sector;
	unsigned int dst_logical_lba, src_logical_lba, lba_num;

	if(dst_logical_address % DATA_SIZE_PER_LOGICAL_SECTOR != 0
		|| src_logical_address % DATA_SIZE_PER_LOGICAL_SECTOR != 0
		|| size % DATA_SIZE_PER_LOGICAL_SECTOR != 0
		|| dst_logical_address > src_logical_address)
		return -EINVAL;

	dst_logical_lba = dst_logical_address / DATA_SIZE_PER_LOGICAL_SECTOR;
	src_logical_lba = src_logical_address / DATA_SIZE_PER_LOGICAL_SECTOR;
	lba_num = size / DATA_SIZE_PER_LOGICAL_SECTOR;

	sem_wait(&spinor_logical_semphore);

	__spinor_logical_flush();
	for(; lba_num > 0; lba_num--, dst_logical_lba++, src_logical_lba++)
	{
    	watchdog_clear();
		dst_logical_blk = dst_logical_lba / SPINOR_BLOCK_SECTORS;
		dst_sector = dst_logical_lba % SPINOR_BLOCK_SECTORS;
		src_logical_blk = src_logical_lba / SPINOR_BLOCK_SECTORS;
		src_sector = src_logical_lba % SPINOR_BLOCK_SECTORS;

		if(mode == 1)
		{
			__spinor_logical_read_sector_from_log_block_or_data_block(dst_logical_blk, dst_sector);
			if(spinor_logical->tmp->extend == src_logical_lba)
				continue;
			mode = 0;
		}
		__spinor_logical_switch_log_block(dst_logical_blk);
		__spinor_logical_check_and_update_log_block(dst_logical_blk, dst_sector);
		__spinor_logical_read_sector_from_log_block_or_data_block(src_logical_blk, src_sector);
		__spinor_logical_write_sector_to_log_block(dst_sector, src_logical_lba);
	}

	sem_post(&spinor_logical_semphore);
	return 0;
}

int __spinor_logical_write(unsigned int logical_address, char *buf, unsigned int size)
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

		__spinor_logical_switch_log_block(logical_blk);
		__spinor_logical_check_and_update_log_block(logical_blk, sector);

		if(start_in_sector != 0 || size_in_sector != DATA_SIZE_PER_LOGICAL_SECTOR)
			__spinor_logical_read_sector_in_datablock(logical_blk, sector);

		memcpy(tmp->info.data + start_in_sector, buf, size_in_sector);
		if(start_in_sector + size_in_sector < DATA_SIZE_PER_LOGICAL_SECTOR)
			spinor_logical->log_info->last_write_sector = sector;
		else
			__spinor_logical_write_sector_to_log_block(sector, -1);

		logical_address += size_in_sector;
		buf += size_in_sector;
		size -= size_in_sector;
	}
	return 0;
}

static int __spinor_logical_cache_write(unsigned int logical_address, char *buf, unsigned int size)
{
	unsigned int cached_address, write_size, start_in_sector;
	struct spinor_logical_sector *tmp;

	if(spinor_logical->log_info->last_write_sector == -1)
		return 0;

	tmp = spinor_logical->tmp;

	cached_address = (spinor_logical->log_info->logical_blk * DATA_SIZE_PER_LOGICAL_BLOCK) + (spinor_logical->log_info->last_write_sector * DATA_SIZE_PER_LOGICAL_SECTOR);
	if(logical_address < cached_address || logical_address >= cached_address + DATA_SIZE_PER_LOGICAL_SECTOR)
	{
		__spinor_logical_flush();
		return 0;
	}

	start_in_sector = logical_address % DATA_SIZE_PER_LOGICAL_SECTOR;
	write_size = DATA_SIZE_PER_LOGICAL_SECTOR - start_in_sector;
	if(write_size > size)
		write_size = size;

	memcpy(tmp->info.data + start_in_sector, buf, write_size);

	if(start_in_sector + write_size == DATA_SIZE_PER_LOGICAL_SECTOR)
		__spinor_logical_flush();

	return write_size;
}

int spinor_logical_write(unsigned int logical_address, char *buf, unsigned int size, int mode)
{
	int ret;

	if(size == 0)
		return 0;

	sem_wait(&spinor_logical_semphore);

	if(mode == 0)
	{
		ret = __spinor_logical_cache_write(logical_address, buf, size);
		if(ret != 0)
		{
			logical_address += ret;
			buf += ret;
			size -= ret;
		}
		if(size > 0)
			ret = __spinor_logical_write(logical_address, buf, size);
		else
			ret = 0;
	}
	else
	{
		if((size & 0x1ff) || (logical_address & 0x1ff)
			|| (logical_address / SPINOR_BLOCK_SIZE) < spinor_logical->physical_phyblk
			|| ((logical_address + size + SPINOR_BLOCK_SIZE - 1) / SPINOR_BLOCK_SIZE) > spinor_logical->physical_phyblk + spinor_logical->physical_blknum)
		{
			ret = -EPERM;
		}
		else
		{
#ifdef CONFIG_ESD_EN
			char *tmp_buf = malloc(512);

			if(tmp_buf)
			{
				spinor_read(logical_address >> 9, tmp_buf, 1, 0);
				if(__spinor_buf_is_not_all_ff((void*)tmp_buf, 512 / 8))
				{
					ACT_LOG_ID_INF(ALF_STR_spinor_logical_write__ERROR_D_NOT_0XFF_ALL, 1, logical_address);
					except_reboot();
				}
			}
#endif

			spinor_basic_program_sectors(logical_address >> 9, buf, size >> 9, 0);

#ifdef CONFIG_ESD_EN
			if(tmp_buf)
			{
				spinor_read(logical_address >> 9, tmp_buf, 1, 0);
				if(memcmp(tmp_buf, buf, 512) != 0)
				{
					ACT_LOG_ID_INF(ALF_STR_spinor_logical_write__ERROR_D_WRITE_AND_RE, 1, logical_address);
					except_reboot();
				}

				free(tmp_buf);
			}
#endif
			ret = 0;
		}
	}
	sem_post(&spinor_logical_semphore);

	return ret;
}

