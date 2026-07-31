#include <errno.h>
#include <kernel.h>
#include <linker/sections.h>
#include <flash.h>
#include <soc.h>
#include "xspi_mem_acts.h"
#include "xspi_nor_acts.h"

//#define CONFIG_FLASH_WRITE_PROTECT_TEST


#define NOR_STATUS1_MASK (0x1f<<2)  /*bp4-bp0  status1 bit6-bit2*/
#define NOR_STATUS2_MASK (0x1<<6)  /*cmp   status2  bit6*/

/**********NOR GD25Q32E  2MB  ****************/
#define GD25Q32E_NOR_CHIPID	0x1640c8  	/*GD25Q32E*/
#define GD25Q32E_CMP0_VAL		0
#define GD25Q32E_CMP0_PROTECT_16KB	0x1b /*0-16KB*/
#define GD25Q32E_CMP0_PROTECT_64KB	0x9  /*0-64KB*/
#define GD25Q32E_CMP0_PROTECT_128KB	0xa  /*0-128KB*/
#define GD25Q32E_CMP0_PROTECT_256KB	0xb  /*0-256KB*/
#define GD25Q32E_CMP0_PROTECT_512KB	0xc  /*0-512KB*/
#define GD25Q32E_CMP0_PROTECT_1MB 	0xd  /*0-1MB*/
#define GD25Q32E_CMP0_PROTECT_2MB 	0xe  /*0-2MB*/
#define GD25Q32E_CMP0_PROTECT_4MB 	0xf  /*0-4MB*/
#define GD25Q32E_CMP1_PROTECT_BLOCK_0_62 0x1

#define GD25Q32E_NOR_BP_STATUS1 (GD25Q32E_CMP0_PROTECT_16KB<<2)

#define GD25Q32E_NOR_CMP_STATUS2 (GD25Q32E_CMP0_VAL<<6)
#define GD25Q32E_NOR_CMP1_STATUS2 (1<<6)

struct nor_wp_info {
	unsigned int chipid;
	unsigned char wp_status1_val;
	unsigned char wp_status1_mask;
	unsigned char wp_status2_val;
	unsigned char wp_status2_mask;
};

const struct nor_wp_info  g_nor_wp_info[] = {
	{GD25Q32E_NOR_CHIPID, GD25Q32E_NOR_BP_STATUS1, NOR_STATUS1_MASK, GD25Q32E_NOR_CMP_STATUS2, NOR_STATUS2_MASK},
};

const struct nor_wp_info  g_nor_wp_region_info[] = {
	{GD25Q32E_NOR_CHIPID, GD25Q32E_CMP0_PROTECT_16KB<<2, 		NOR_STATUS1_MASK, GD25Q32E_NOR_CMP_STATUS2, NOR_STATUS2_MASK},
	{GD25Q32E_NOR_CHIPID, GD25Q32E_CMP0_PROTECT_2MB<<2, 		NOR_STATUS1_MASK, GD25Q32E_NOR_CMP_STATUS2, NOR_STATUS2_MASK},
	{GD25Q32E_NOR_CHIPID, GD25Q32E_CMP1_PROTECT_BLOCK_0_62<<2, 	NOR_STATUS1_MASK, GD25Q32E_NOR_CMP1_STATUS2, NOR_STATUS2_MASK},
};


static __ramfunc void nor_write_status(struct xspi_nor_info *sni, u8_t status1, u8_t status2)
{
	u8_t status[2];
	//unsigned int flag;
	status[0] = status1;
	status[1] = status2;
	//flag = sni->spi.flag;
	//sni->spi.flag &= ~SPI_FLAG_NO_IRQ_LOCK;  //wait ready
	xspi_nor_write_status(sni, XSPI_NOR_CMD_WRITE_STATUS2, &status2, 1);
	xspi_nor_write_status(sni, XSPI_NOR_CMD_WRITE_STATUS,  &status1,  1);
	xspi_nor_write_status(sni, XSPI_NOR_CMD_WRITE_STATUS,  status, 2);
	//sni->spi.flag =  flag;
}

static __ramfunc void nor_read_status(struct xspi_nor_info *sni, u8_t *status1, u8_t *status2)
{
	*status1 = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS);
	*status2 = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS2);
}

static void xspi_nor_protect_handle(struct xspi_nor_info *sni, const struct nor_wp_info *wp, bool benable)
{
	u32_t flags;
	u8_t status1, status2;
	u8_t set_status1 = 0, set_status2 = 0;
	u8_t read_status1, read_status2;
	u8_t val1;
	flags = irq_lock();

	nor_read_status(sni, &status1, &status2);

	val1 = wp->wp_status1_mask & status1;
	if(benable){
		if(val1 !=  wp->wp_status1_val){// enable protect
			set_status1 =  ((~wp->wp_status1_mask) & status1) | wp->wp_status1_val;
			set_status2 =  ((~wp->wp_status2_mask) & status2) | wp->wp_status2_val;
			nor_write_status(sni, set_status1, set_status2);
		}
	}else{
		if(val1 !=	0){// diable protect
			set_status1 =  (~wp->wp_status1_mask) & status1;
			set_status2 =  (~wp->wp_status2_mask) & status2;
			nor_write_status(sni, set_status1, set_status2);
		}
	}
	nor_read_status(sni, &read_status1, &read_status2);
	irq_unlock(flags);

	printk("status1-2=0x%x,0x%x(0x%x, 0x%x)\n", status1, status2, wp->wp_status1_val, wp->wp_status2_val);

	if(set_status1 || set_status2){
		printk("set status1-2=0x%x,0x%x\n", set_status1, set_status2);
	}

	printk("read again status1-2=0x%x,0x%x\n", read_status1, read_status2);

}


int nor_write_protection(const struct device *dev, bool enable)
{
	struct xspi_nor_info *sni = (struct xspi_nor_info *)dev->driver_data;
	const struct nor_wp_info *wp;
	int i;
	for(i = 0; i < ARRAY_SIZE(g_nor_wp_info); i++){
		wp = &g_nor_wp_info[i];
		if(wp->chipid == sni->chipid)
			xspi_nor_protect_handle(sni, wp, enable);
	}
	return 0;
}

int nor_write_protection_region(const struct device *dev, int region_id)
{
	struct xspi_nor_info *sni = (struct xspi_nor_info *)dev->driver_data;
	const struct nor_wp_info *wp;
	int i;

	for(i = 0; i < ARRAY_SIZE(g_nor_wp_region_info); i++){
		wp = &g_nor_wp_region_info[i];
		if(wp->chipid == sni->chipid){
			break;
		}
	}

	if(i == ARRAY_SIZE(g_nor_wp_region_info)){
		return -EIO;
	}

	printk("flash protect set region %d\n", region_id);

	if(region_id == FLASH_PROTECT_REGION_NONE){
		xspi_nor_protect_handle(sni, wp, false);
	}else if(region_id == FLASH_PROTECT_REGION_BOOT){
		xspi_nor_protect_handle(sni, &wp[0], true);
	}else if(region_id == FLASH_PROTECT_REGION_BOOT_AND_SYS){
		xspi_nor_protect_handle(sni, &wp[1], true);
	}else if(region_id == FLASH_PROTECT_REGION_ALL_EXCLUDE_NVRAM){
		xspi_nor_protect_handle(sni, &wp[2], true);
	}else{
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_FLASH_WRITE_PROTECT_TEST

#include <mem_manager.h>

void flash_write_test_sub(struct device *flash_device, uint32_t test_addr, uint32_t test_size)
{
	uint32_t i;
	static int test_cnt = 0;
	uint8_t *write_buffer = mem_malloc(0x1000);

	flash_erase(flash_device, test_addr, test_size);

	flash_read(flash_device, test_addr, write_buffer, test_size);

	for(i = 0; i < test_size; i++){
		if(write_buffer[i] != 0xff){
			printk("erase test failed %d\n", i);
			break;
		}
	}

	if(i == test_size){
		printk("erase test ok\n");
	}

	for(i = 0; i < test_size; i++){
		write_buffer[i] = (uint8_t)(i + test_cnt);
	}

	flash_write(flash_device, test_addr, write_buffer, test_size);

	flash_read(flash_device, test_addr, write_buffer, test_size);

	for(i = 0; i < test_size; i++){
		if(write_buffer[i] != (uint8_t)(i + test_cnt)){
			printk("write test failed %d %d\n", i, write_buffer[i]);
			break;
		}
	}

	if(i == test_size){
		printk("write test ok\n");
	}

	mem_free(write_buffer);

	printk("test cnt %d\n", test_cnt);

	test_cnt++;
}

void flash_write_protection_test(struct device *flash_device, uint32_t test_addr, uint32_t test_size)
{
	printk("\n flash write protect disable test (addr:0x%x size:%d bytes)\n", test_addr, test_size);

	flash_write_protection_set(flash_device, false);

	flash_write_test_sub(flash_device, test_addr, test_size);

	printk("\n flash write protect enable test (addr:0x%x size:%d bytes)\n", test_addr, test_size);

	flash_write_protection_set(flash_device, true);

	flash_write_test_sub(flash_device, test_addr, test_size);

}

void flash_write_protection_region_test(struct device *flash_device, uint32_t test_addr, uint32_t test_size)
{
	printk("\n flash write protect region none test (addr:0x%x size:%d bytes) test \n", test_addr, test_size);

	flash_write_protection_region_set(flash_device, FLASH_PROTECT_REGION_NONE);

	flash_write_test_sub(flash_device, test_addr, test_size);

	printk("\n flash write protect region boot test (addr:0x%x size:%d bytes) test \n", test_addr, test_size);

	flash_write_protection_region_set(flash_device, FLASH_PROTECT_REGION_BOOT);

	flash_write_test_sub(flash_device, test_addr, test_size);

	printk("\n flash write protect region boot&sys test (addr:0x%x size:%d bytes) test \n", test_addr, test_size);

	flash_write_protection_region_set(flash_device, FLASH_PROTECT_REGION_BOOT_AND_SYS);

	flash_write_test_sub(flash_device, test_addr, test_size);

	printk("\n flash write protect region all exclude nvram test (addr:0x%x size:%d bytes) test \n", test_addr, test_size);

	flash_write_protection_region_set(flash_device, FLASH_PROTECT_REGION_ALL_EXCLUDE_NVRAM);

	flash_write_test_sub(flash_device, test_addr, test_size);

}

#include <init.h>

int flash_test(struct device *dev)
{
	uint8_t *backup_ptr = (uint8_t *)0x60000;

	struct device *flash_device = device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);

	if (flash_device){

		flash_read(flash_device, 0x3000, backup_ptr, 0x1000);

		flash_read(flash_device, 0x1e0000, backup_ptr + 0x1000, 0x1000);

		flash_read(flash_device, 0x26c000, backup_ptr + 0x2000, 0x1000);

		flash_read(flash_device, 0x3f0000, backup_ptr + 0x3000, 0x1000);

		flash_write_protection_test(flash_device, 0x3000, 0x1000);

		flash_write_protection_test(flash_device, 0x26c000, 0x1000);

		flash_write_protection_region_test(flash_device, 0x3000, 0x1000);

		flash_write_protection_region_test(flash_device, 0x1e0000, 0x1000);

		flash_write_protection_region_test(flash_device, 0x26c000, 0x1000);

		flash_write_protection_region_test(flash_device, 0x3f0000, 0x1000);

		flash_write_protection_set(flash_device, false);

		flash_erase(flash_device, 0x3000, 0x1000);

		flash_write(flash_device, 0x3000, backup_ptr, 0x1000);

		flash_erase(flash_device, 0x1e0000, 0x1000);

		flash_write(flash_device, 0x1e0000, backup_ptr + 0x1000, 0x1000);

		flash_erase(flash_device, 0x26c000, 0x1000);

		flash_write(flash_device, 0x26c000, backup_ptr + 0x2000, 0x1000);

		flash_erase(flash_device, 0x3f0000, 0x1000);

		flash_write(flash_device, 0x3f0000, backup_ptr + 0x3000, 0x1000);
	}

	return 0;
}

SYS_INIT(flash_test, POST_KERNEL, 90);

#endif

