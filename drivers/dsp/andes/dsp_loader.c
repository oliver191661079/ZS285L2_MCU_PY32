/*
 * Copyright (c) 1997-2015, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <zephyr.h>
#include <misc/util.h>
#include <dsp.h>
#include "dsp_inner.h"
#include "dsp_image.h"
#include <logging/sys_log.h>

int dsp_acts_request_image(struct device *dev, const struct dsp_imageinfo *image, int type)
{
	uint32_t entry_point = UINT32_MAX;

	struct dsp_acts_data *dsp_data = dev->driver_data;

	if (type >= ARRAY_SIZE(dsp_data->images)) {
		printk("%s: dsp image <%s> invalid type %d\n", __func__, image->name, type);
		return -EINVAL;
	}

	if (load_dsp_image(image->ptr, image->size, &entry_point)) {
		printk("%s: cannot load dsp image <%s>\n", __func__, image->name);
		return -EINVAL;
	}
#if 0
	printk("%s: dsp image <%s> loaded, type=%d, entry_point=0x%x\n",
			__func__, image->name, type, entry_point);
#endif

	if (type == DSP_IMAGE_MAIN) {
		clear_dsp_pageaddr();

		/* set DSP_VECTOR_ADDRESS */
		set_dsp_vector_addr(entry_point);
	}

	memcpy(&dsp_data->images[type], image, sizeof(*image));

	dsp_data->images[type].entry_point = entry_point;
	return 0;
}

int dsp_acts_release_image(struct device *dev, int type)
{
	struct dsp_acts_data *dsp_data = dev->driver_data;

	if (type >= ARRAY_SIZE(dsp_data->images))
		return -EINVAL;

#if 0
	printk("%s: dsp image <%s> free, type=%d\n",
		__func__, dsp_data->images[type].name, type);
#endif

	memset(&dsp_data->images[type], 0, sizeof(dsp_data->images[0]));

	return 0;
}

static void dsp_acts_handle_invalid_epc(struct device *dev, u32_t epc)
{
	const struct dsp_acts_config *dsp_cfg = dev->config->config_info;
	struct dsp_protocol_userinfo *dsp_userinfo = dsp_cfg->dsp_userinfo;

	dsp_userinfo->error_code = DSP_BAD_COMMAND;

	dsp_poweroff(dev);
}

static int dsp_acts_is_valid_epc(u32_t epc)
{
	u32_t real_address = IMG_BANK_INNER_ADDR(epc);

	/* if bank flag is zero,epc is invalid */
	if(!(epc & (1 << 29))){
		return false;
	}

	real_address = dsp_code_to_mcu_address(real_address);

	if(real_address >= CONFIG_DSP_SRAM_BASE_ADDRESS \
		&& real_address <= CONFIG_DSP_SRAM_END_ADDRESS){
		return true;
	}else{
		return false;
	}
}

int dsp_acts_handle_image_pagemiss(struct device *dev, u32_t epc)
{
	unsigned int bank_group = IMG_BANK_GROUP(epc);
	unsigned int bank_index = IMG_BANK_INDEX(epc);
	unsigned int bank_no = BANK_NO(bank_group, bank_index);
	unsigned int type = IMG_TYPE(epc);
	struct dsp_acts_data *dsp_data = dev->driver_data;
	const struct dsp_imageinfo *image;

	if (type >= DSP_NUM_IMAGE_TYPES) {
		printk("%s: invalid epc 0x%08x\n", __func__, epc);
		dsp_acts_handle_invalid_epc(dev, epc);
		return -EFAULT;
	}

	image = &dsp_data->images[type];

	/* if image->name is null, dsp lib is invalid*/
	if (!image || !image->name || !dsp_acts_is_valid_epc(epc)){
		printk("%s: invalid epc 0x%08x\n", __func__, epc);
		dsp_acts_handle_invalid_epc(dev, epc);
		return -EFAULT;
	}

	printk("page miss (0x%08x, 0x%x, %u, %u)\n",
		   epc, type, bank_group, bank_index);

	if (load_dsp_image_bank(image->ptr, image->size, bank_no)) {
		printk("cannot load bank[%u] for epc 0x%08x\n", bank_no, epc);
		dsp_acts_handle_invalid_epc(dev, epc);
		return DSP_FAIL;
	}

	/* epc is word unit */
	unsigned int page_addr = (epc * 2) & 0xfff80000;

	//check tlb is bit19~bit31
	if (sys_read32(DSP_PAGE_ADDR0 + (bank_group << 2)) == page_addr)
		return -EBUSY;
	else
		sys_write32(page_addr, DSP_PAGE_ADDR0 + (bank_group << 2));

	return DSP_DONE;
}

int dsp_acts_handle_image_pageflush(struct device *dev, u32_t epc)
{
	unsigned int bank_group = IMG_BANK_GROUP(epc);
	unsigned int bank_index = IMG_BANK_INDEX(epc);
	unsigned int type = IMG_TYPE(epc);
	struct dsp_acts_data *dsp_data = dev->driver_data;
	const struct dsp_imageinfo *image;

	if (type >= DSP_NUM_IMAGE_TYPES) {
		printk("%s: invalid epc 0x%08x\n", __func__, epc);
		dsp_acts_handle_invalid_epc(dev, epc);
		return -EFAULT;
	}

	image = &dsp_data->images[type];

	/* if image->name is null, dsp lib is invalid*/
	if (!image || !image->name || !dsp_acts_is_valid_epc(epc)){
		printk("%s: invalid epc 0x%08x\n", __func__, epc);
		dsp_acts_handle_invalid_epc(dev, epc);
		return -EFAULT;
	}

	printk("%s: dsp image <%s> page flush, epc=0x%08x, type=%x, bank_group=%u, bank_index=%u\n",
		   __func__, image->name, epc, type, bank_group, bank_index);

    sys_write32(0, DSP_PAGE_ADDR0 + (bank_group << 2));

    return DSP_DONE;
}
