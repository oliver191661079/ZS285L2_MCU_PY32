#include "spinor_logical_inner.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SPINOR_LOGICAL_REBUILD

static void __spinor_logical_rebuild_log_info(void)
{
	unsigned char i, sector;
	short block_lba;
	struct spinor_logical_sector *tmp;

	for(i = 0; i < SPINOR_MAX_LOG_NUM; i++)
	{
		if(spinor_logical->log_table[i].phyblk != BLOCK_NULL)
		{
			spinor_logical->log_info = &spinor_logical->log_table[i];

			tmp = spinor_logical->tmp;
			block_lba = spinor_logical->log_info->phyblk * SPINOR_BLOCK_SECTORS;
			for(sector = 0; sector < SPINOR_BLOCK_SECTORS; sector++)
			{
				spinor_read(block_lba + sector, (void*)tmp, 1, 0);
				if(tmp->checksum == (unsigned short)-1 && tmp->extend == (unsigned short)-1)
					continue;
				spinor_read(block_lba + sector, (void*)tmp, 1, 1);
				if(sector == 0)
					spinor_logical->log_info->sector_1st = tmp->info.sector;
				if(__spinor_logical_sector_checksum() != tmp->checksum)
				{
					pr_err("breakdown when program phyblk%d, erase!\n", spinor_logical->log_info->phyblk);
					spinor_basic_block_erase(block_lba * 512);
					spinor_logical->free_table[spinor_logical->free_in_index++] = spinor_logical->log_info->phyblk;
					spinor_logical->log_info->phyblk = BLOCK_NULL;
					break;
				}
				__log_block_set_sector_status(sector);
			}
			}
	}
}

static void __spinor_update_free_block_and_erase_not_ff_block(int mode)
{
	int i, count, phyblk, flag;

	ACT_LOG_ID_INF(ALF_STR___spinor_update_free_block_and_erase_not_ff_block__UPDATINGN, 0);
	count = spinor_logical->free_in_index - spinor_logical->free_out_index;
	count = count > 0 ? count : count + spinor_logical->total_blknum;
	while(count-- > 0)
	{
		flag = spinor_logical->tmp_table[spinor_logical->free_out_index];
		phyblk = spinor_logical->free_table[spinor_logical->free_out_index];
		if(++spinor_logical->free_out_index >= spinor_logical->total_blknum)
			spinor_logical->free_out_index = 0;
		if(phyblk >= spinor_logical->logical_phyblk
			&& phyblk < spinor_logical->logical_phyblk + spinor_logical->logical_blknum)
		{
		    watchdog_clear();
			if(spinor_logical->free_in_index >= spinor_logical->total_blknum)
				spinor_logical->free_in_index = 0;
			spinor_logical->free_table[spinor_logical->free_in_index++] = phyblk;
			if(flag)
				spinor_basic_block_erase(phyblk * SPINOR_BLOCK_SIZE);
			else if(mode == 1)
			{
				for(i = 1; i < SPINOR_BLOCK_SECTORS; i++)
				{
					spinor_read((phyblk * SPINOR_BLOCK_SECTORS) + i, (void*)spinor_logical->tmp, 1, 0);
					if(__spinor_buf_is_not_all_ff((void*)spinor_logical->tmp, 512 / 8))
					{
						spinor_basic_block_erase(phyblk * SPINOR_BLOCK_SIZE);
						break;
					}
				}
			}
		}
	}
	spinor_logical->dirty_in_index = spinor_logical->free_in_index;

	ACT_LOG_ID_INF(ALF_STR___spinor_update_free_block_and_erase_not_ff_block__COMPLETEN, 0);
}

static void __spinor_trim_all_logical_block(void)
{
	int i, phyblk;

	ACT_LOG_ID_INF(ALF_STR___spinor_trim_all_logical_block__TRIMING_IT_MAY_COSTE, 0);
	__spinor_update_free_block_and_erase_not_ff_block(1);
	for(i = 0; i < SPINOR_MAX_LOG_NUM; i++)
	{
		if(spinor_logical->log_table[i].phyblk != BLOCK_NULL)
		{
			spinor_basic_block_erase(spinor_logical->log_table[i].phyblk * SPINOR_BLOCK_SIZE);
			spinor_logical->log_table[i].phyblk = BLOCK_NULL;
		}
	}
	for(i = 0; i < spinor_logical->total_blknum; i++)
	{
		if(spinor_logical->logical_table[i].phyblk != BLOCK_NULL)
			spinor_basic_block_erase(spinor_logical->logical_table[i].phyblk * SPINOR_BLOCK_SIZE);
	}

	memset(spinor_logical->logical_table, 0, spinor_logical->total_blknum * sizeof(spinor_logical->logical_table[0]));
	memset(spinor_logical->tmp_table, 0, spinor_logical->total_blknum);

	spinor_logical->free_in_index = 0;
	spinor_logical->free_out_index = 0;
	for(phyblk = spinor_logical->logical_phyblk, i = 0; i < spinor_logical->total_blknum; i++, phyblk++)
		spinor_logical->free_table[spinor_logical->free_in_index++] = phyblk;
	spinor_logical->dirty_in_index = spinor_logical->free_in_index;

	ACT_LOG_ID_INF(ALF_STR___spinor_trim_all_logical_block__TRIMING_FINISHEDN, 0);
}


/*逻辑分区的头16个字节保存了更新后的逻辑分区和物理分区信息*/
static void __spinor_logical_update_logical_physical_area(void)
{
	spinor_partition_t spinor_partition;

	__spinor_logical_read(0, (void*)&spinor_partition, sizeof(spinor_partition));
	if(spinor_partition.magic != 0x70617274
		|| spinor_partition.firmware_flash_lba_offset != firmware_flash_lba_offset
		|| spinor_partition.first_flag == 1)
	{
		if(spinor_partition.magic != 0x70617274)
			__spinor_trim_all_logical_block();
		else
			__spinor_update_free_block_and_erase_not_ff_block(spinor_partition.first_flag);

		spinor_partition.magic = 0x70617274;
		spinor_partition.logical_phyblk = spinor_logical->logical_phyblk;
		spinor_partition.logical_blknum = spinor_logical->logical_blknum;
		spinor_partition.physical_phyblk = spinor_logical->physical_phyblk;
		spinor_partition.physical_blknum = spinor_logical->physical_blknum;
		spinor_partition.firmware_flash_lba_offset = firmware_flash_lba_offset;
		spinor_partition.first_flag = 0;
		__spinor_logical_write(0, (void*)&spinor_partition, sizeof(spinor_partition));
		__spinor_logical_flush();
		return;
	}

	spinor_logical->logical_phyblk = spinor_partition.logical_phyblk;
	spinor_logical->logical_blknum = spinor_partition.logical_blknum;
	spinor_logical->physical_phyblk = spinor_partition.physical_phyblk;
	spinor_logical->physical_blknum = spinor_partition.physical_blknum;
	__spinor_update_free_block_and_erase_not_ff_block(0);
}

static int __spinor_logical_get_block_type(unsigned char phyblk)
{
	int i;
	unsigned int phyblk_lba;
	unsigned char sectors[3] = {0, 1, SPINOR_BLOCK_SECTORS - 1};

	phyblk_lba = phyblk * SPINOR_BLOCK_SECTORS;
	for(i = 0; i < sizeof(sectors) / sizeof(sectors[0]); i++)
	{
		spinor_read(phyblk_lba + sectors[i], (void*)spinor_logical->tmp, 1, 0);
		if(!__spinor_buf_is_not_all_ff((void*)spinor_logical->tmp, 512 / 8))
			continue;
		spinor_read(phyblk_lba + sectors[i], (void*)spinor_logical->tmp, 1, 1);
		if(__spinor_logical_sector_checksum() == spinor_logical->tmp->checksum)
			return 1;	//返回1，是逻辑分区的有效块
		return -1;	//有部分sector不是全0xff，且不是逻辑分区块，需要擦除后再放入空闲块
	}

	return 0;
}

void spinor_logical_rebuild_table(void)
{
	unsigned char phyblk, i, logical_blk, j;
	signed char blk_type;
	struct logical_block_info *logical_block_info;
	struct spinor_logical_sector *tmp = spinor_logical->tmp;

	for(phyblk = spinor_logical->logical_phyblk, i = 0; i < spinor_logical->total_blknum; i++, phyblk++)
	{
		blk_type = __spinor_logical_get_block_type(phyblk);

		if(blk_type <= 0)
		{
			if(blk_type == 0)
				spinor_logical->tmp_table[spinor_logical->free_in_index] = 0;
			else
				spinor_logical->tmp_table[spinor_logical->free_in_index] = 1;
			spinor_logical->free_table[spinor_logical->free_in_index++] = phyblk;
			continue;
		}

		logical_blk = tmp->info.logical_blk;
		logical_block_info = &spinor_logical->logical_table[logical_blk];
		if(logical_block_info->phyblk != BLOCK_NULL)
		{
			for(j = 0; j < SPINOR_MAX_LOG_NUM; j++)
			{
				if(spinor_logical->log_table[j].phyblk == BLOCK_NULL)
				{
					spinor_logical->log_info = &spinor_logical->log_table[j];
					break;
				}
			}

			spinor_logical->log_info->logical_blk = logical_blk;
			if((unsigned char)(logical_block_info->blkseq + 1) == tmp->info.blkseq)	/*phyblk是日志块*/
			{
				spinor_logical->log_info->phyblk = phyblk;
				spinor_logical->log_info->blkseq = tmp->info.blkseq;
				continue;
			}
			spinor_logical->log_info->phyblk = logical_block_info->phyblk;	/*原logical_table的phyblk是日志块*/
			spinor_logical->log_info->blkseq = logical_block_info->blkseq;
		}
		logical_block_info->phyblk = phyblk;
		logical_block_info->blkseq = tmp->info.blkseq;
	}

	__spinor_logical_rebuild_log_info();
	tmp->info.logical_blk = -1;

	__spinor_logical_update_logical_physical_area();
}

