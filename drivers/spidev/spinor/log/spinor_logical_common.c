#include "spinor_logical_inner.h"
#include <word_checksum.h>

#define OTA_DEBUG

struct spinor_logical_info *spinor_logical;
sem_t spinor_logical_semphore;

unsigned short __spinor_logical_sector_checksum(void)
{
    union
    {
        unsigned int word_checksum;
        unsigned short half_checksum[2];
    } val;

	val.word_checksum = calculate_word_checksum((void*)&spinor_logical->tmp->info, sizeof(spinor_logical->tmp->info) / 4) + 0x1234;
    return val.half_checksum[0] + val.half_checksum[1];
}

void __log_block_set_sector_status(int sector)
{
	spinor_logical->log_info->sector_map[sector >> 5] |= (1 << (sector & 0x1f));
}

int __log_block_get_sector_status(unsigned char sector)
{
	return (spinor_logical->log_info->sector_map[sector >> 5] & (1 << (sector & 0x1f)));
}

static void __log_block_clear_sector_status(void)
{
	spinor_logical->log_info->sector_map[0] = 0;
	spinor_logical->log_info->sector_map[1] = 0;
	spinor_logical->log_info->sector_map[2] = 0;
	spinor_logical->log_info->sector_map[3] = 0;
}

unsigned char __spinor_logical_get_log_block_phy_sector(unsigned char sector)
{
	if(sector == 0)
		sector = spinor_logical->log_info->sector_1st;
	else if(sector == spinor_logical->log_info->sector_1st)
		sector = 0;

	return sector;
}

unsigned char __spinor_logical_get_free_phy_block(void)
{
	unsigned char phyblk, dirty = 0, i;

	if(spinor_logical->free_out_index >= spinor_logical->total_blknum)
		spinor_logical->free_out_index = 0;
	if(spinor_logical->free_out_index == spinor_logical->free_in_index)
    {
    	struct spinor_block_info *log_info;

        for(i = 0; i < SPINOR_MAX_LOG_NUM; i++)
        {
            if(spinor_logical->log_table[i].phyblk != BLOCK_NULL
                && (&spinor_logical->log_table[i]) != spinor_logical->log_info)
            {
                log_info = spinor_logical->log_info;
                spinor_logical->log_info = &spinor_logical->log_table[i];
                __spinor_logical_merge_log_block_and_replace_datablock(1);
                spinor_logical->log_info = log_info;
                break;
            }
        }
        if(i == SPINOR_MAX_LOG_NUM)
		    panic("no free block!\n");
    }
	if(spinor_logical->free_out_index == spinor_logical->dirty_in_index)
	{
		spinor_logical->dirty_in_index++;
		if(spinor_logical->dirty_in_index >= spinor_logical->total_blknum)
			spinor_logical->dirty_in_index = 0;
		dirty = 1;
	}
	phyblk = spinor_logical->free_table[spinor_logical->free_out_index++];
	if(dirty)
		spinor_basic_block_erase(phyblk * SPINOR_BLOCK_SIZE);
#if 0
	else
	{
		void * buf = malloc(512);

		spinor_basic_fast_read_sectors(phyblk * SPINOR_BLOCK_SECTORS, buf, 1, 0);
		if(__spinor_buf_is_not_all_ff(buf, 512 / 8))
		{
			panic("error, new block %d is not all 0xff\n", phyblk);
		}
		free(buf);
	}
#endif
	return phyblk;
}

void __spinor_logical_put_free_phy_block(unsigned char phyblk, unsigned char need_recycle)
{
	if(!need_recycle)
	{
		spinor_basic_block_erase(phyblk * SPINOR_BLOCK_SIZE);
		return;
	}
	spinor_basic_program_sectors(phyblk * SPINOR_BLOCK_SECTORS, NULL, 1, 0);

	if(spinor_logical->free_in_index >= spinor_logical->total_blknum)
		spinor_logical->free_in_index = 0;
	if(spinor_logical->dirty_in_index >= spinor_logical->total_blknum)
		spinor_logical->dirty_in_index = spinor_logical->free_in_index;
	spinor_logical->free_table[spinor_logical->free_in_index++] = phyblk;
}

void __spinor_logical_new_log_block(unsigned char logical_blk)
{
	spinor_logical->log_info->sector_1st = 0;
	spinor_logical->log_info->phyblk = __spinor_logical_get_free_phy_block();
	spinor_logical->log_info->blkseq = spinor_logical->logical_table[logical_blk].blkseq + 1;
	spinor_logical->log_info->logical_blk = logical_blk;
	__log_block_clear_sector_status();
}

