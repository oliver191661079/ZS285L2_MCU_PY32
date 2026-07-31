/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <zephyr.h>
#include <soc.h>
#include <image.h>
#include <img_load.h>
#include <sdfs.h>
#include <logging/sys_log.h>
#include <property_manager.h>
#include <esd_manager.h>


typedef struct{
	u8_t id;
	char* name;
} bin_test_info_t;

//#0:none bin test; 1:fcc; 2:produce; 3:srrc
typedef enum {
	BIN_TEST_ID_NONE = 0,
	BIN_TEST_ID_FCC,
	BIN_TEST_ID_NSM_UART,
	BIN_TEST_ID_SRRC,
	BIN_TEST_ID_NSM_SPPBLE,
	BIN_TEST_ID_MAX,
} bin_test_id_e;

const bin_test_info_t bin_info[]=
{
	{.id = BIN_TEST_ID_NONE,	.name = NULL},
	{.id = BIN_TEST_ID_FCC,		.name = "pt1.bin"},
	{.id = BIN_TEST_ID_NSM_UART,	.name = "pd1.bin"},
	{.id = BIN_TEST_ID_NSM_SPPBLE,	.name = "pd1.bin"},
	{.id = BIN_TEST_ID_SRRC,	.name = "srrc.bin"},
};


uint32_t img_code_load(char *file_name)
{
	struct sd_file *file_p;
	image_head_t *p_img;
	uint32_t address = 0;

	file_p = sd_fopen(file_name);

	if (file_p) {
		p_img = (image_head_t *) file_p->start;
		printk
		    ("name %s, load_addr 0x%x, entery 0x%x, size 0x%x\n",
		     p_img->name, (int)p_img->load_addr,
		     (int)p_img->entry, (file_p->size));

		printk("version 0x%04x\n", p_img->version);

		if ((file_p->size)) {
			memcpy((void *)p_img->load_addr,
			       (void *)file_p->start, file_p->size);

		}
		address = (uint32_t) p_img->entry;

		sd_fclose(file_p);
	} else {
		SYS_LOG_INF("cannot open %s.", file_name);
		while(1)
		{
			k_sleep(1000);
		}
	}

	return address;
}

bin_test_info_t *img_get_test_bin_info(void)
{
	int bin_id = property_get_int(CFG_BIN_TEST_ID, -1);
	int i = 0;

	if(bin_id <= BIN_TEST_ID_NONE || bin_id >= BIN_TEST_ID_MAX){

#if defined(CONFIG_NVRAM_USER_STORAGE_EXT_FLASH) && defined(CONFIG_ESD_MANAGER)
		esd_manager_restore_scene(TAG_BT_TEST_BIN, (uint8_t *)&bin_id, 1);
		if(bin_id <= BIN_TEST_ID_NONE || bin_id >= BIN_TEST_ID_MAX){
			return NULL;
		}
#else
		return NULL;
#endif
	}

	for( i = 0; i < ARRAY_SIZE(bin_info); i++)
	{
		if(bin_info[i].id == bin_id)
		{
			break;
		}
	}

	if(i >= ARRAY_SIZE(bin_info))
	{
		return NULL;
	}

	return (bin_test_info_t *)&bin_info[i];
}

u8_t img_code_is_nsm_sppble_test(void)
{
	bin_test_info_t *p = img_get_test_bin_info();

	if(!p)
		return 0;

	if(p->id == BIN_TEST_ID_NSM_SPPBLE)
		return 1;
	else
		return 0;
}

u32_t img_code_load_nsm(void)
{

	bin_test_info_t * bin_info_p = img_get_test_bin_info();
	u32_t img_code_entry = 0;

	if(!bin_info_p)
		return 0;

	if(bin_info_p->id == BIN_TEST_ID_NSM_SPPBLE)
	{
		SYS_LOG_INF("load %s\n", bin_info_p->name);
		int lock = irq_lock();

		img_code_entry = img_code_load(bin_info_p->name);
		if (img_code_entry) {
			sys_write32(0x0, WD_CTL);
			SYS_LOG_INF("irq_lock, goto 0x%x", (int)img_code_entry);
			u32_t * nsm_use_sppble = (u32_t *) 0x60504;
			*nsm_use_sppble = 1;
			SYS_LOG_INF("sdk nsm_use_sppble 0x%x", *nsm_use_sppble);

			u32_t* rc_filter_circuit_bypass = (u32_t *) 0x60508;
#ifdef CONFIG_CHIP_RC_FILTER_CIRCUIT_BYPASS
			*rc_filter_circuit_bypass = 1;
#else
			*rc_filter_circuit_bypass = 0;
#endif
			SYS_LOG_INF("sdk rc_filter_circuit_bypass 0x%x", *rc_filter_circuit_bypass);
			k_sleep(100);
		} else {
			irq_unlock(lock);
			SYS_LOG_INF("Cannot load %s\n", bin_info_p->name);
		}
	}

	return img_code_entry;
}

void img_code_manage(void)
{
	void (*img_code_entry)(void);

	bin_test_info_t * bin_info_p = img_get_test_bin_info();

	if(!bin_info_p)
		return;

	if(bin_info_p->id == BIN_TEST_ID_NSM_SPPBLE)
	{
		//spp ble need to connect and recieve cmd then enter test_bin
		return;
	}

#ifndef CONFIG_ACTIONS_IMG_TEST_ALWAYS
	u8_t mode = '0';
	SYS_LOG_INF("reset bin test id\n");

	int ret = property_set(CFG_BIN_TEST_ID, &mode, 1);

#if defined(CONFIG_NVRAM_USER_STORAGE_EXT_FLASH) && defined(CONFIG_ESD_MANAGER)
	mode -= '0';
	esd_manager_save_scene(TAG_BT_TEST_BIN, (uint8_t *)&mode, 1);
#endif

	if (ret < 0) {
		return;
	}
	property_flush(CFG_BIN_TEST_ID);
#endif

	SYS_LOG_INF("load %s\n", bin_info_p->name);

	img_code_entry = (void (*)(void))(img_code_load(bin_info_p->name));
	if (NULL != img_code_entry) {
		sys_write32(0x0, WD_CTL);
		SYS_LOG_INF("irq_lock, and goto 0x%x", (int)img_code_entry);
		irq_lock();
		u32_t* rc_filter_circuit_bypass = (u32_t *) 0x60508;
#ifdef CONFIG_CHIP_RC_FILTER_CIRCUIT_BYPASS
		*rc_filter_circuit_bypass = 1;
#else
		*rc_filter_circuit_bypass = 0;
#endif
		SYS_LOG_INF("sdk rc_filter_circuit_bypass 0x%x", *rc_filter_circuit_bypass);
		img_code_entry();
	} else {
		SYS_LOG_INF("Cannot load %s\n", bin_info_p->name);
	}
}

int run_test_image(void)
{

	img_code_manage();

	soc_pm_rtc_bak_write(0, 0);

	return 0;
}
