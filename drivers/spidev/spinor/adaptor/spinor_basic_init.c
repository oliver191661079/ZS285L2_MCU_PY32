#include <sdk.h>
#include <spi/spinor.h>
#include <brom.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SPINOR_BASIC_INIT

//#define SHOW_STATUS_REGISTER

#define SPINOR_WRITE_STATUS_COMMAND_01  0x01
#define SPINOR_READ_STATUS_COMMAND_05   0x05

extern unsigned int firmware_flash_lba_offset, firmware_flash_sectors;
extern unsigned char storage_type;

extern void spinor_basic_write_enable(void);

#define SHOW_STATUS_REGISTER


extern void spi_write_status_register(uint8 cmd, uint32 data, uint32 write_twice, uint32 status_16bit);

#ifdef SHOW_STATUS_REGISTER

extern void spi_read_status_register(uint8 cmd, uint8 *data);

static void spinor_show_status_register(void)
{
    unsigned char status[2];

    status[0] = 0xff;
    status[1] = 0xff;
    if(spinor_id.rwsr_2nd.read_cmd != 0)
    {
        if(spinor_id.rwsr_2nd.read_cmd != SPINOR_READ_STATUS_COMMAND_05)
        {
			status[0] = spinor_read_status(SPINOR_READ_STATUS_COMMAND_05);
			status[1] = spinor_read_status(spinor_id.rwsr_2nd.read_cmd);
        }
    }
    else
    {
		spi_read_status_register(SPINOR_READ_STATUS_COMMAND_05, status);
    }

    ACT_LOG_ID_INF(ALF_STR_spinor_show_status_register__SPINOR_STATUS_REGIST, 2, status[0], status[1]);
}
#endif

static void spinor_basic_enable_protect_area(int index)
{
	unsigned int status;

    if(spinor_id.rwsr_2nd.write_cmd != 0)
    {
        if(spinor_id.rwsr_2nd.write_cmd == SPINOR_WRITE_STATUS_COMMAND_01)
        {
			status = spinor_id.spm.bp[index].bp_val << 2;
			status |= spinor_id.spm.bp[index].cmp_val << 6;
			spinor_write_status(SPINOR_WRITE_STATUS_COMMAND_01, status, 2);
        }
        else
        {
			status = spinor_id.spm.bp[index].bp_val << 2;
			spinor_write_status(SPINOR_WRITE_STATUS_COMMAND_01, status, 1);
			status = spinor_id.spm.bp[index].cmp_val << 6;
			spinor_write_status(spinor_id.rwsr_2nd.write_cmd, status, 1);
        }
    }
    else
    {
		status = spinor_id.spm.bp[index].bp_val << 2;
#ifdef SHOW_STATUS_REGISTER
		//status[1] = 0;
#endif
		spinor_write_status(SPINOR_WRITE_STATUS_COMMAND_01, status, 1);
    }

#ifdef SHOW_STATUS_REGISTER
	ACT_LOG_ID_INF(ALF_STR_spinor_basic_enable_protect_area__SPINOR_WRITE_STATUS_, 1, status);
    spinor_show_status_register();
#endif
}

static int spinor_basic_protect_init(void)
{
    int i, blk_start, blk_num, blk_last;
    unsigned char disable_spi_write_protect;

    if(fwinfo_read(&disable_spi_write_protect, offsetof(fwinfo_t, disable_spi_write_protect), 1) == 0
        && disable_spi_write_protect == 1)
    {
        pr_warn("Norfalsh spm diabled by fwinfo\n");
        return 0;
    }

    blk_start = 1;
    blk_num = (((blk_start * SPINOR_BLOCK_SECTORS) + firmware_flash_sectors + SPINOR_BLOCK_SECTORS - 1) / SPINOR_BLOCK_SECTORS) - blk_start;

    if(spinor_id.spm.bp_size == 0)
    {
        pr_err("Norfalsh not support spm\n");
        return -EPERM;
    }
    if(blk_start >= spinor_id.spm.bp[0].blk_num)
    {
        pr_err("Norfalsh no such spm area(%d,%d)\n", blk_start, blk_num);
        return -ENXIO;
    }

    blk_last = blk_start + blk_num - 1;
    //从小到大查找，找到满足要求的最小保护区
    for(i = spinor_id.spm.bp_size - 1; i >= 0; i--)
    {
        if(blk_last < spinor_id.spm.bp[i].blk_num)
            break;
    }
    //即使保护不全，也总比没有保护的要好
    if(i < 0)
    {
        i = 0;
        pr_warn("want protect(%d,%d), but only(%d,%d)\n"
            , blk_start, blk_num
            , 0, spinor_id.spm.bp[i].blk_num);
    }
    if(spinor_id.block_num - spinor_id.spm.bp[i].blk_num < 2)
    {
        pr_err("not enough free blocks for spm\n");
        return -ENOSPC;
    }

    spinor_protect_block_start = 0;
    spinor_protect_block_num = spinor_id.spm.bp[i].blk_num;
    spinor_protect_index = i;

	//spinor_4B_mode = spinor_id.addr_4B_mode;

    return 0;
}

int spinor_basic_protect_enable(void)
{
    return 0;

    if(spinor_protect_index == 0xff)
    {
        pr_err("Norfalsh not support spm\n");
        return -EPERM;
    }

    if(firmware_flash_lba_offset > SPINOR_BLOCK_SECTORS)
    {
        pr_err("not protect in recovery mode\n");
        return -EACCES;
    }

    spinor_basic_enable_protect_area(spinor_protect_index);
    return 0;
}

void spinor_basic_protect_disable(void)
{
	unsigned char status = 0;

	return;

    if(spinor_id.rwsr_2nd.write_cmd != 0)
    {
        if(spinor_id.rwsr_2nd.write_cmd == SPINOR_WRITE_STATUS_COMMAND_01)
        {
			spinor_write_status(SPINOR_WRITE_STATUS_COMMAND_01, status, 2);
        }
        else
        {
			spinor_write_status(SPINOR_WRITE_STATUS_COMMAND_01, status, 1);
			spinor_write_status(spinor_id.rwsr_2nd.write_cmd, status, 1);
        }
    }
    else
    {
		spinor_write_status(SPINOR_WRITE_STATUS_COMMAND_01, status, 1);
    }
#ifdef SHOW_STATUS_REGISTER
    spinor_show_status_register();
#endif
}

int spinor_basic_init(void)
{
#if 0
	mbrec_head_t *mbrec_head;

	mbrec_head = malloc(512);
	if(mbrec_head == NULL)
		return -ENOMEM;

	spinor_read(0, (void*)mbrec_head, 1, 1);
    spinor_id = mbrec_head->spinor_id;
    free(mbrec_head);

	spinor_basic_protect_init();

	/*若当前启动分区在flash前半部，则启动写保护*/
	if(storage_type == STORAGE_TYPE_NOR && firmware_flash_lba_offset == SPINOR_BLOCK_SECTORS)
    {
		//spinor_basic_protect_enable();
    }
	/*若当前启动分区在flash后半部，则取消写保护*/
    else
    {
		//spinor_basic_protect_disable();
    }
    return 0;
#else
    //默认2M  
    spinor_id.block_num = 2 * 1024 / 64;
#endif
}


