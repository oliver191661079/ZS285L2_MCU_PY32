#include <sdk.h>
#include <spi/spinor.h>
#include <libc/semaphore.h>

#define SPINOR_MAX_LOG_NUM 2

struct spinor_block_info
{
	unsigned char logical_blk;
	unsigned char phyblk;
	unsigned char blkseq;
	unsigned char sector_1st;
	unsigned int  sector_map[4];

	short last_write_sector;
};

struct logical_block_info
{
	unsigned char phyblk;
	unsigned char blkseq;
};

struct spinor_logical_info
{
	unsigned char logical_phyblk;
	unsigned char logical_blknum;
	unsigned char physical_phyblk;
	unsigned char physical_blknum;

	unsigned char total_blknum;
	unsigned char free_in_index;
	unsigned char free_out_index;
	unsigned char dirty_in_index;
	unsigned char inited_count;

	int logical_size;

	struct spinor_block_info *log_info;
	struct spinor_block_info log_table[SPINOR_MAX_LOG_NUM];
	struct logical_block_info *logical_table;
	unsigned char *free_table;
	struct spinor_logical_sector *tmp;
	unsigned char *tmp_table;
};

#define BLOCK_NULL (0)	/*spinor block 0是bootloader,不可能被用作逻辑块*/
#define DATA_SIZE_PER_LOGICAL_BLOCK (DATA_SIZE_PER_LOGICAL_SECTOR * SPINOR_BLOCK_SECTORS)

#define LOGICAL_SECTOR_PER_PAGE	(PAGE_SIZE / sizeof(struct spinor_logical_sector))
#define DATA_SIZE_PER_LOGICAL_PAGE (LOGICAL_SECTOR_PER_PAGE * DATA_SIZE_PER_LOGICAL_SECTOR)

#define CHECK_SUM_SIZE (sizeof(((struct spinor_logical_sector *)0)->info) / 4)

unsigned short __spinor_logical_sector_checksum(void);
void __log_block_set_sector_status(int sector);
int __log_block_get_sector_status(unsigned char sector);
unsigned char __spinor_logical_get_free_phy_block(void);
void __spinor_logical_put_free_phy_block(unsigned char phyblk, unsigned char need_recycle);

extern struct spinor_logical_info *spinor_logical;
extern sem_t spinor_logical_semphore;
extern unsigned int firmware_flash_lba_offset, firmware_flash_sectors;

int __spinor_logical_read(unsigned int logical_address, char *buf, unsigned int size);
int __spinor_logical_write(unsigned int logical_address, char *buf, unsigned int size);
void __spinor_logical_read_one_sector(unsigned int lba);
void __spinor_logical_read_sector_in_datablock(unsigned char logical_blk, unsigned char sector);
void __spinor_logical_read_sector_from_log_block_or_data_block(unsigned char logical_blk, unsigned char sector);

void __spinor_logical_merge_log_block_and_replace_datablock(unsigned char need_recycle);
void __spinor_logical_new_log_block(unsigned char logical_blk);
void __spinor_logical_flush(void);
bool __spinor_buf_is_not_all_ff(unsigned long long *buf, int uint64_len);
unsigned char __spinor_logical_get_log_block_phy_sector(unsigned char sector);

