/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include <misc/__assert.h>
#include <errno.h>
#include <init.h>
#include <device.h>
#include <flash.h>
#include <os_common_api.h>
#include <msg_manager.h>
#include "psram.h"

#define PSRAM_MAX_BLOCK_BASE 0
#define PSRAM_MAX_BLOCK_NUM  1

#define PSRAM_MIN_BLOCK_BASE (PSRAM_MAX_BLOCK_BASE + PSRAM_MAX_BLOCK_SIZE * PSRAM_MAX_BLOCK_NUM)
#define PSRAM_MIN_BLOCK_NUM  8

#define PSRAM_MAX_BLOCK_MASK   (0xff00)
#define PSRAM_MAX_BLOCK_BIT(n) (1 << (8 + n))
#define PSRAM_MIN_BLOCK_MASK   (0x00ff)
#define PSRAM_MIN_BLOCK_BIT(n) (1 << n)

/** structure of psram*/
struct psram
{
	/** dev of psram*/
	struct device *psram_dev;
	/** alloc flag */
	u32_t alloc_bit_mask;
	/** mutex for psram */
	os_mutex mutex;
};
#ifdef CONFIG_PSRAM
static struct psram * psram_manager = NULL;
#endif
int psram_init(void)
{
#ifdef CONFIG_PSRAM
	psram_manager = mem_malloc(sizeof(struct psram));

	if(!psram_manager)
	{
		goto exit;
	}

	psram_manager->psram_dev = device_get_binding(CONFIG_SPI_PSRAM_DEV_NAME);

	if (!psram_manager->psram_dev)
		return -ENODEV;

	psram_manager->alloc_bit_mask = 0;

	k_mutex_init(&psram_manager->mutex);

	SYS_LOG_INF(" ok\n");
exit:
#endif
	return 0;
}

int psram_read(u32_t offset, u8_t *buff, u32_t len)
{
#ifdef CONFIG_PSRAM
	if (!psram_manager || !psram_manager->psram_dev)
		return -ENODEV;

	if (flash_read(psram_manager->psram_dev, offset, buff, len) != 0) {
		return -EIO;
	}
#endif
	//print_buffer(buff, 1, len, 16, -1);
	return 0;
}

int psram_write(u32_t offset, u8_t *buff, u32_t len)
{
#ifdef CONFIG_PSRAM
	if (!psram_manager || !psram_manager->psram_dev)
		return -ENODEV;

	if (flash_write(psram_manager->psram_dev, offset, buff, len) != 0) {
		return -EIO;
	}
#endif
	//print_buffer(buff, 1, len, 16, -1);
	return 0;
}

int psram_alloc(u32_t size)
{
	int ret = 0;
#ifdef CONFIG_PSRAM
	if(!psram_manager)
	{
		psram_init();
	}

	if (!psram_manager || !psram_manager->psram_dev)
		return -ENODEV;

	os_mutex_lock(&psram_manager->mutex, OS_FOREVER);

	switch(size)
	{
		case PSRAM_MAX_BLOCK_SIZE:
			for(int i = 0; i < PSRAM_MAX_BLOCK_NUM; i++)
			{
				if((psram_manager->alloc_bit_mask & PSRAM_MAX_BLOCK_BIT(i)) == 0)
				{
					ret = PSRAM_MAX_BLOCK_BASE + i * PSRAM_MAX_BLOCK_SIZE;
					psram_manager->alloc_bit_mask |= PSRAM_MAX_BLOCK_BIT(i);
					goto exit;
				}
			}

			/** music cahce psram we alway used not depend on alloc */
			SYS_LOG_INF("alloc already allocated psram block (base=0x%x)\n", PSRAM_MAX_BLOCK_BASE);
			ret = PSRAM_MAX_BLOCK_BASE;
			break;
		default:
			if(size <= PSRAM_MIN_BLOCK_SIZE)
			{
				for(int i = 0; i < PSRAM_MIN_BLOCK_NUM; i++)
				{
					if((psram_manager->alloc_bit_mask & PSRAM_MIN_BLOCK_BIT(i)) == 0)
					{
						ret = PSRAM_MIN_BLOCK_BASE + i * PSRAM_MIN_BLOCK_SIZE;
						psram_manager->alloc_bit_mask |= PSRAM_MIN_BLOCK_BIT(i);
						goto exit;
					}
				}
			} else {
				ret = -EINVAL;
			}
			break;
	}

exit:
	os_mutex_unlock(&psram_manager->mutex);
#endif
	SYS_LOG_INF(" size 0x%x addr 0x%x \n", size, ret);
	return ret;
}

int psram_free(u32_t offset)
{
	int ret = 0;
#ifdef CONFIG_PSRAM
	int i = 0;

	if (!psram_manager || !psram_manager->psram_dev)
		return -ENODEV;

	os_mutex_lock(&psram_manager->mutex, OS_FOREVER);

	for(i = 0; i < PSRAM_MAX_BLOCK_NUM; i++)
	{
		if((psram_manager->alloc_bit_mask & PSRAM_MAX_BLOCK_BIT(i)) != 0
			&& offset == PSRAM_MAX_BLOCK_BASE + i * PSRAM_MAX_BLOCK_SIZE)
		{
			psram_manager->alloc_bit_mask &= (~PSRAM_MAX_BLOCK_BIT(i));
			goto exit;
		}
	}

	for(i = 0; i < PSRAM_MIN_BLOCK_NUM; i++)
	{
		if((psram_manager->alloc_bit_mask & PSRAM_MIN_BLOCK_BIT(i)) != 0
			&& offset == PSRAM_MIN_BLOCK_BASE + i * PSRAM_MIN_BLOCK_SIZE)
		{
			psram_manager->alloc_bit_mask &= (~PSRAM_MIN_BLOCK_BIT(i));
			goto exit;
		}
	}

	ret = -EINVAL;

exit:
	if(ret)
	{
		SYS_LOG_ERR("mem_free failed  0x%x \n",offset);
	}
	os_mutex_unlock(&psram_manager->mutex);
#endif
	return ret;
}


