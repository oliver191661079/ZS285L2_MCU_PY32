#include "spinor_logical_inner.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SPINOR_LOGICAL_INIT

extern void spinor_logical_rebuild_table(void);

static void __spinor_logical_struct_release(void)
{
	if(!spinor_logical)
		return;
	
	if(spinor_logical->tmp)
		free(spinor_logical->tmp);
	if(spinor_logical->logical_table)
		free(spinor_logical->logical_table);
	if(spinor_logical->free_table)
		free(spinor_logical->free_table);
	if(spinor_logical->tmp_table)
		free(spinor_logical->tmp_table);

	free(spinor_logical);
	spinor_logical = NULL;
}

void spinor_logical_load_info(int *phyblk, int *total_blknum)
{
	/*若当前启动分区在flash前半部*/
	if(firmware_flash_lba_offset == SPINOR_BLOCK_SECTORS)
	{
		*phyblk = (firmware_flash_lba_offset + firmware_flash_sectors + SPINOR_BLOCK_SECTORS - 1) / SPINOR_BLOCK_SECTORS;
        /*如果启动了写保护，则重新调整逻辑区域位置*/
        if(spinor_protect_block_num != 0 && *phyblk < spinor_protect_block_start + spinor_protect_block_num)
        {
            *phyblk = spinor_protect_block_start + spinor_protect_block_num;
        }
		*total_blknum = spinor_id.block_num - *phyblk;
	}
	/*若当前启动分区在flash后半部*/
	else
	{
		*phyblk = 1;
		*total_blknum = (firmware_flash_lba_offset / SPINOR_BLOCK_SECTORS) - *phyblk;
	}

	if(*phyblk > 255)
	{
        *phyblk = 255;
	}

	if(*total_blknum > 255)
	{
        *total_blknum = 255;
	}
	printk("t%d,f(%d,%d),v(%d,%d),p(%d,%d)\n", spinor_id.block_num
        , firmware_flash_lba_offset / SPINOR_BLOCK_SECTORS, (firmware_flash_sectors + SPINOR_BLOCK_SECTORS - 1) / SPINOR_BLOCK_SECTORS
        , *phyblk, *total_blknum
        , spinor_protect_block_start, spinor_protect_block_num);
}

static int __spinor_logical_struct_create(void)
{
	int i, phyblk, total_blknum;

	spinor_logical_load_info(&phyblk, &total_blknum);
	if(phyblk > 255 || total_blknum > 255)
		return -EINVAL;
	if(total_blknum < 2)
    {
        ACT_LOG_ID_INF(ALF_STR___spinor_logical_struct_create__FREE_BLOCK_NUM_DN, 1, total_blknum);
		return -ENOSPC;	/*要实现nor的写操作，至少需要2个空闲block*/
    }

	spinor_logical = malloc(sizeof(*spinor_logical));
	if(!spinor_logical)
		return -ENOMEM;
	memset(spinor_logical, 0, sizeof(*spinor_logical));

	spinor_logical->total_blknum = total_blknum;
	spinor_logical->tmp = malloc(sizeof(*spinor_logical->tmp));
	spinor_logical->logical_table = malloc(total_blknum * sizeof(struct logical_block_info));
	spinor_logical->free_table = malloc(total_blknum);
	spinor_logical->tmp_table = malloc(total_blknum);
	

	if(!spinor_logical->tmp
		|| !spinor_logical->logical_table || !spinor_logical->free_table || !spinor_logical->tmp_table)
	{
		__spinor_logical_struct_release();
		return -ENOMEM;
	}
	memset(spinor_logical->logical_table, 0, total_blknum * sizeof(struct logical_block_info));
	
	spinor_logical->logical_phyblk = phyblk;
	spinor_logical->logical_blknum = total_blknum;
	spinor_logical->logical_size = (total_blknum - 1) * DATA_SIZE_PER_LOGICAL_BLOCK;
	/*如果逻辑区起始位置大于1，则表示当前物理连续启动区位于nor的开头，物理连续可写区位于nor的末尾。
	  如果逻辑区起始位置等于1，则相反*/
	if(spinor_logical->logical_phyblk > 1)
		spinor_logical->physical_phyblk = phyblk + total_blknum;
	else
		spinor_logical->physical_phyblk = phyblk;

	for(i = 0; i < SPINOR_MAX_LOG_NUM; i++)
		spinor_logical->log_table[i].last_write_sector = -1;
	spinor_logical->log_info = &spinor_logical->log_table[0];
	return 0;
}


/*返回逻辑区长度,若返回负值,则初始化失败*/
int spinor_logical_init(void)
{
	int result;

	sem_dynamic_create(&spinor_logical_semphore, 1);
	
	sem_wait(&spinor_logical_semphore);
	if(spinor_logical == NULL)
	{
		result = __spinor_logical_struct_create();
		if(result)
		{
			sem_post(&spinor_logical_semphore);
			return result;
		}
		spinor_logical_rebuild_table();
		free(spinor_logical->tmp_table);
		spinor_logical->tmp_table = NULL;
	}

	spinor_logical->inited_count++;
	result = spinor_logical->logical_size;
	sem_post(&spinor_logical_semphore);

	return result;
}

void spinor_logical_release(void)
{
	sem_wait(&spinor_logical_semphore);
	if(spinor_logical != NULL)
	{
		spinor_logical->inited_count--;
		if(spinor_logical->inited_count == 0)
		{
			__spinor_logical_struct_release();
			spinor_logical = NULL;
		}
	}
	sem_post(&spinor_logical_semphore);
}

int spinor_logical_get_logical_size(void)
{
	return spinor_logical->logical_size;
}


