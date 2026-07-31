#include <kernel.h>
#include <init.h>
#include <string.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_PARTITION
#include <soc.h>
#include <partition.h>

#define PARTITION_TABLE_MAGIC		0x54504341	// 'ACPT'

struct partition_table {
	u32_t magic;
	u16_t version;
	u16_t table_size;
	u16_t part_cnt;
	u16_t part_entry_size;
	u8_t reserved1[4];

	struct partition_entry parts[MAX_PARTITION_COUNT];
	u8_t Reserved2[4];
	u32_t table_crc;
};

static const struct partition_table *g_part_table;

void partition_dump(void)
{
	const struct partition_entry *part;
	char part_name[8];
	int i;

	SYS_LOG_INF();
	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		part = &g_part_table->parts[i];

		/* name field to str */
		memcpy(part_name, part->name, 8);
		//part_name[8] = '\0';
        part_name[7] = '\0';

		if (part->type != 0) {
		printk("part[%d]: %s type %d, file_id %d, mirror_id %d, offset 0x%x, size 0x%x flag 0x%x\n", i, \
			part_name, part->type, part->file_id, part->mirror_id, part->offset, part->size, part->flag);
		}
	}
}

int partition_get_max_file_size(const struct partition_entry *part)
{
	if (!part)
		return -EINVAL;

	return (part->size - (part->file_offset - part->offset));
}

const struct partition_entry *partition_find_part(u32_t nor_phy_addr)
{
	const struct partition_entry *part;
	unsigned int i;

	 __ASSERT_NO_MSG(g_part_table);

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		part = &g_part_table->parts[i];

		if ((part->offset <= nor_phy_addr) &&
			((part->offset + part->size) > nor_phy_addr)) {

			return part;
		}
	}

	return NULL;
}

const struct partition_entry *partition_get_part_by_id(u8_t part_id)
{
	 __ASSERT_NO_MSG(g_part_table);

	if (part_id >= MAX_PARTITION_COUNT)
		return NULL;

	return &g_part_table->parts[part_id];
}

const struct partition_entry *partition_get_current_part(void)
{
	u32_t cur_nor_addr;

	 __ASSERT_NO_MSG(g_part_table);

	cur_nor_addr = soc_memctrl_cpu_to_nor_phy_addr(CONFIG_FLASH_BASE_ADDRESS);

	return partition_find_part(cur_nor_addr);
}

u8_t partition_get_current_mirror_id(void)
{
	const struct partition_entry *part;

	part = partition_get_current_part();
	if (part == NULL)
		return -1;

	return part->mirror_id;
}

u32_t partition_get_partition_nor_addr(uint32_t vaddr)
{
	u32_t cur_nor_addr;

	 __ASSERT_NO_MSG(g_part_table);

	cur_nor_addr = soc_memctrl_cpu_to_nor_phy_addr(vaddr);

	return cur_nor_addr;
}

u8_t partition_get_boot_mirror_id(void)
{
	const struct partition_entry *part;

	part = partition_find_part(partition_get_partition_nor_addr(SOC_BOOT_SYSTEM_PARAM_VADDR));

	if (part == NULL)
		return -1;

	return part->mirror_id;
}


u8_t partition_get_current_file_id(void)
{
	const struct partition_entry *part;

	part = partition_get_current_part();
	if (part == NULL)
		return -1;

	return part->file_id;
}

int partition_is_mirror_part(const struct partition_entry *part)
{
	u8_t mirror_id;

	 __ASSERT_NO_MSG(g_part_table);

	mirror_id = partition_get_current_mirror_id();

	if ((part->mirror_id != mirror_id) && (part->mirror_id != PARTITION_MIRROR_ID_INVALID)) {
		return 1;
	}

	return 0;
}

int partition_is_param_part(const struct partition_entry *part)
{
	 __ASSERT_NO_MSG(g_part_table);

	if (part->type == PARTITION_TYPE_PARAM)
		return 1;

	return 0;
}

int partition_is_boot_part(const struct partition_entry *part)
{
	 __ASSERT_NO_MSG(g_part_table);

	if (part->type == PARTITION_TYPE_BOOT)
		return 1;

	return 0;
}

int partition_has_mirror_part(u8_t file_id)
{
	const struct partition_entry *part;
	int i, idx;

	idx = -1;

	 __ASSERT_NO_MSG(g_part_table);

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		part = &g_part_table->parts[i];

		if (part->file_id == file_id) {
			if(idx == -1){
				idx = i;
			}else{
				return true;
			}
		}
	}

	return false;

}

const struct partition_entry *partition_get_part(u8_t file_id)
{
	const struct partition_entry *part;
	int i;
	int has_mirror_part = partition_has_mirror_part(file_id);

	 __ASSERT_NO_MSG(g_part_table);

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		part = &g_part_table->parts[i];

		if ((part->file_id == file_id)) {
			if(has_mirror_part && partition_is_mirror_part(part)){
				printk("part file id %d is mirror part\n", file_id);
				return NULL;
			}
			return part;
		}
	}

	return NULL;
}


const struct partition_entry *partition_get_temp_part(void)
{
	const struct partition_entry *temp_part;

	temp_part = partition_get_part(PARTITION_FILE_ID_OTA_TEMP);
	if (temp_part == NULL || (temp_part->type != PARTITION_TYPE_TEMP)) {
		SYS_LOG_ERR("cannot found temp part\n");
		//return -ENOENT;
		return NULL;
	}

	return temp_part;
}

const struct partition_entry *partition_get_mirror_part(u8_t file_id)
{
	const struct partition_entry *part;
	int i;

	 __ASSERT_NO_MSG(g_part_table);

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		part = &g_part_table->parts[i];
		if ((part->file_id == file_id) && partition_is_mirror_part(part)) {
			/* found */
			SYS_LOG_INF("found write part %d for file_id=0x%x", i, file_id);
			return part;
		}
	}

	return NULL;
}

const struct partition_entry *partition_get_boot_mirror_part(u8_t file_id)
{
	const struct partition_entry *part;
	int i;
	int boot_mirror_id;

	__ASSERT_NO_MSG(g_part_table);

	boot_mirror_id = partition_get_boot_mirror_id();

	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		part = &g_part_table->parts[i];
		if ((part->file_id == file_id) && \
			((part->mirror_id != boot_mirror_id) && (part->mirror_id != PARTITION_MIRROR_ID_INVALID))) {
			/* found */
			SYS_LOG_INF("found write part %d offset %x mirror id %d for file_id=0x%x", i, part->file_offset,\
			boot_mirror_id, file_id);
			return part;
		}
	}

	return NULL;
}


int partition_file_mapping(u8_t file_id, u32_t vaddr)
{
	const struct partition_entry *part;
	int err, crc_is_enabled;

	part = partition_get_part(file_id);
	if (part == NULL) {
		SYS_LOG_ERR("cannot found file_id %d\n", file_id);
		return -ENOENT;
	}

	crc_is_enabled = part->flag & PARTITION_FLAG_ENABLE_CRC ? 1 : 0;

	err = soc_memctrl_mapping(vaddr, part->file_offset, crc_is_enabled);
	if (err) {
		SYS_LOG_ERR("cannot mapping file_id %d\n", file_id);
		return err;
	}

	return 0;
}

static int partition_init(struct device *dev)
{
	u32_t cksum;

	g_part_table = (const struct partition_table *)soc_boot_get_part_tbl_addr();
	if (g_part_table == NULL ||
	    g_part_table->magic != PARTITION_TABLE_MAGIC) {
		SYS_LOG_ERR("partition table (%p) has wrong magic\n", g_part_table);
		goto failed;
	}

	cksum = utils_crc32(0, (const uint8_t *)g_part_table, sizeof(struct partition_table) - 4);
	if (cksum != g_part_table->table_crc) {
		SYS_LOG_ERR("partition table (%p) checksum error\n", g_part_table);
		goto failed;
	}

	partition_dump();

	return 0;

failed:
	g_part_table = NULL;
	return 0;
}

SYS_INIT(partition_init, PRE_KERNEL_1, 20);

