#include <sdk.h>
#include <spi/spinor.h>
#include <brom.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SPINOR_BASIC_WRITE

spinor_id_t spinor_id;
unsigned char spinor_protect_block_start, spinor_protect_block_num, spinor_protect_index = 0xff;
unsigned char spinor_4B_mode;

extern unsigned int firmware_flash_lba_offset, firmware_flash_sectors, write_protect_magic;
extern unsigned char storage_type;

#define enableFuncSect 0

/*SST25LF080A/SST25LF040A/SST25LF020A只支持0x52,不支持0xd8*/
#define SPINOR_BLOCK_ERASE_COMMAND 0xD8
/*SST的0x02命令只能写一个字节,连续写需要使用其特殊的AAI指令*/
#define SPINOR_PROGRAM_COMMAND 0x02

extern unsigned char program_cmd[4];
extern void *program_page_buf;
extern unsigned char program_random;

extern void spinor_basic_write_enable(void);
//extern void spinor_basic_wait_transfrer_complete(void);
extern void spinor_basic_program_one_page(void);


static void __spinor_basic_firmware_protected(unsigned int lba, unsigned int sectors)
{
    /*如果写操作block0的前3个扇区，或是当前系统存放的物理扇区，则报错死机*/
    if(write_protect_magic != 0x70726f74 && storage_type == STORAGE_TYPE_NOR
        && ((lba <= 0 && lba + sectors > 0)
            || (lba <= firmware_flash_lba_offset && lba + sectors > firmware_flash_lba_offset)
            || (lba >= 0 && lba < 3)
            || (lba >= firmware_flash_lba_offset && lba < firmware_flash_lba_offset + firmware_flash_sectors)))
    {
        panic("forbit write or erase area(%d,%d)\n", lba, lba + sectors);
    }
}

void spinor_basic_block_erase(unsigned int block_addr)
{
    //int i;
    ACT_LOG_ID_INF(ALF_STR_spinor_basic_block_erase__ERASE_DN, 1, block_addr / SPINOR_BLOCK_SIZE);

	__spinor_basic_firmware_protected(block_addr / 512, SPINOR_BLOCK_SECTORS);

    spinor_erase(block_addr, SPINOR_BLOCK_SIZE);

}

void spinor_basic_program_sectors(unsigned int lba, void *sectors_buf, int sectors, int random)
{
    __spinor_basic_firmware_protected(lba, sectors);

    spinor_write(lba, sectors_buf, sectors, random);
}

