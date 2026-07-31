#include "spinor_logical_inner.h"


void spinor_logical_flush(void)
{
	sem_wait(&spinor_logical_semphore);
	__spinor_logical_flush();
	sem_post(&spinor_logical_semphore);
}

void spinor_logical_swap(void)
{
    sem_wait(&spinor_logical_semphore);
	__spinor_logical_merge_log_block_and_replace_datablock(1);
	sem_post(&spinor_logical_semphore);
}

unsigned char spinor_logical_get_free_phyblock_for_outer(void)
{
	sem_wait(&spinor_logical_semphore);

	__spinor_logical_merge_log_block_and_replace_datablock(1);
	return __spinor_logical_get_free_phy_block();
}

void spinor_logical_put_free_phyblock_from_outer(unsigned char phyblock)
{
	__spinor_logical_put_free_phy_block(phyblock, 1);
	
	sem_post(&spinor_logical_semphore);
}

int spinor_logical_trim_no_used_logical_area(unsigned int trim_start_address)
{
	unsigned char logical_blk, phyblk, flag;
    int i, freeblk_num;

    logical_blk = (trim_start_address + DATA_SIZE_PER_LOGICAL_BLOCK - 1) / DATA_SIZE_PER_LOGICAL_BLOCK;
    if(logical_blk * DATA_SIZE_PER_LOGICAL_BLOCK > spinor_logical->logical_size)
        return -EPERM;
    
	sem_wait(spinor_logical_semphore);
    
    __spinor_logical_merge_log_block_and_replace_datablock(1);
	for(; logical_blk < spinor_logical->total_blknum; logical_blk++)
	{
	    phyblk = spinor_logical->logical_table[logical_blk].phyblk;
		if(phyblk == BLOCK_NULL)
            continue;
        
		__spinor_logical_put_free_phy_block(phyblk, 1);
        spinor_logical->logical_table[logical_blk].phyblk = BLOCK_NULL;
	}

	freeblk_num = spinor_logical->free_in_index - spinor_logical->free_out_index;
	freeblk_num = freeblk_num > 0 ? freeblk_num : freeblk_num + spinor_logical->total_blknum;

	flag = 0;
	for(i = 0; i < freeblk_num; i++)
	{
		if(spinor_logical->free_out_index >= spinor_logical->total_blknum)
			spinor_logical->free_out_index = 0;
		if(spinor_logical->free_out_index == spinor_logical->dirty_in_index)
			flag = 1;
		
		phyblk = spinor_logical->free_table[spinor_logical->free_out_index++];
		if(flag)
			spinor_basic_block_erase(phyblk * SPINOR_BLOCK_SIZE);
        
		if(spinor_logical->free_in_index >= spinor_logical->total_blknum)
			spinor_logical->free_in_index = 0;
		spinor_logical->free_table[spinor_logical->free_in_index++] = phyblk;
	}
    spinor_logical->dirty_in_index = spinor_logical->free_in_index;
    
	sem_post(&spinor_logical_semphore);
    return 0;
}

static void __spinor_logical_delete_last_datablocks(int num)
{
    int i, phyblk;
    
	for(i = spinor_logical->logical_blknum - 2; i >= 0 && num >= 0; i--, num--)
	{
		phyblk = spinor_logical->logical_table[i].phyblk;
        if(phyblk == BLOCK_NULL)
            continue;
        
		spinor_basic_block_erase(phyblk * SPINOR_BLOCK_SIZE);
		if(phyblk >= spinor_protect_block_num)
        {
        	if(spinor_logical->free_in_index >= spinor_logical->total_blknum)
            	spinor_logical->free_in_index = 0;
            spinor_logical->free_table[spinor_logical->free_in_index++] = phyblk;
        }
        spinor_logical->logical_table[i].phyblk = BLOCK_NULL;
    }
}

/*把位于保护区的逻辑块，搬移到非保护区，防止升级完成后再启动时，建表时将保护区的块放到逻辑区*/
int spinor_logical_move_block_in_protect_area_and_return_used_logical_size(void)
{
    int i, phyblk, freeblk_num, moveNum;

	/*若当前启动分区在flash的前半部，或是spinor_protect_block_num==0，则不用检查*/
	if(firmware_flash_lba_offset == SPINOR_BLOCK_SECTORS || spinor_protect_block_num == 0)
        return -1;

	/*若当前启动分区在flash的后半部，并且spinor_protect_block_num > 0*/
    
    /*清除日志块*/
	for(i = 0; i < SPINOR_MAX_LOG_NUM; i++)
	{
		if(spinor_logical->log_table[i].phyblk != BLOCK_NULL)
        {      
    		spinor_logical->log_info = &spinor_logical->log_table[i];
            __spinor_logical_merge_log_block_and_replace_datablock(1);
        }
	}

    /*清除落在保护区内的空闲块*/
	freeblk_num = spinor_logical->free_in_index - spinor_logical->free_out_index;
	freeblk_num = freeblk_num > 0 ? freeblk_num : freeblk_num + spinor_logical->total_blknum;
	while(freeblk_num-- > 0)
	{
		phyblk = __spinor_logical_get_free_phy_block();
		if(phyblk < spinor_protect_block_num) 
            continue;
    	if(spinor_logical->free_in_index >= spinor_logical->total_blknum)
        	spinor_logical->free_in_index = 0;
        spinor_logical->free_table[spinor_logical->free_in_index++] = phyblk;
    }

    /*如果需要移动的块数量大于空闲块，则需要先将末尾的数据块清除为空闲块*/
    moveNum = 0;
	for(i = 0; i < spinor_logical->logical_blknum - 1; i++)
	{
		phyblk = spinor_logical->logical_table[i].phyblk;
		if(phyblk == BLOCK_NULL || phyblk >= spinor_protect_block_num)
			continue;
        moveNum++;
	}
	freeblk_num = spinor_logical->free_in_index - spinor_logical->free_out_index;
	freeblk_num = freeblk_num > 0 ? freeblk_num : freeblk_num + spinor_logical->total_blknum;
    if(moveNum + 1 > freeblk_num)
        __spinor_logical_delete_last_datablocks(moveNum - freeblk_num + 1);  //至少得有一个空闲块
	spinor_logical->dirty_in_index = spinor_logical->free_in_index;

    /*开始移动保护区中的数据块*/
	for(i = 0; i < spinor_logical->logical_blknum - 1; i++)
	{
		phyblk = spinor_logical->logical_table[i].phyblk;
		if(phyblk != BLOCK_NULL && phyblk < spinor_protect_block_num)
        {
    		__spinor_logical_new_log_block(i);
    		__spinor_logical_merge_log_block_and_replace_datablock(0);
        }
	}

    /*剩余已使用的逻辑空间大小*/
	for(i = spinor_logical->logical_blknum - 2; i >= 0; i--)
	{
        if(spinor_logical->logical_table[i].phyblk != BLOCK_NULL)
            break;
    }
    return ((i + 1) * DATA_SIZE_PER_LOGICAL_BLOCK);
}

