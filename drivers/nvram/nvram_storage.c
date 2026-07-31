/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief NVRAM storage
 */

#include <errno.h>
#include <kernel.h>
#include <init.h>
#include <device.h>
#include <flash.h>
#include <string.h>

#define SYS_LOG_DOMAIN "NVRAM"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_NVRAM_STORAGE

#ifdef CONFIG_NVRAM_STORAGE_RAMSIM
#define NVRAM_MEM_SIZE	(CONFIG_NVRAM_FACTORY_REGION_SIZE + \
			 CONFIG_NVRAM_USER_REGION_SIZE)
#define NVRAM_MEM_OFFS	CONFIG_NVRAM_FACTORY_REGION_BASE_ADDR

static u8_t nvram_mem_buf[NVRAM_MEM_SIZE];
#endif

#define NVRAM_STORAGE_READ_SEGMENT_SIZE		256
#define NVRAM_STORAGE_WRITE_SEGMENT_SIZE	32

int nvram_storage_write(struct device *dev, u32_t addr,
			const void *buf, s32_t size)
{
	SYS_LOG_INF("write addr 0x%x len 0x%x", addr, size);

#ifdef CONFIG_NVRAM_STORAGE_RAMSIM
	memcpy(nvram_mem_buf + addr - NVRAM_MEM_OFFS, buf, len);
	return 0;
#else
	int err;
	int wlen = NVRAM_STORAGE_WRITE_SEGMENT_SIZE;

	while (size > 0) {
		if (size < NVRAM_STORAGE_WRITE_SEGMENT_SIZE)
			wlen = size;

		err = flash_write(dev, addr, buf, wlen);
		if (err < 0) {
			SYS_LOG_ERR("write error %d, addr 0x%x, buf %p, size %d",
				err, addr, buf, size);
			return -EIO;
		}

		addr += wlen;
		buf += wlen;
		size -= wlen;
	}

	return 0;
#endif
}

int nvram_storage_read(struct device *dev, u32_t addr,
				     void *buf, s32_t size)
{
	SYS_LOG_DBG("read addr 0x%x len 0x%x", addr, size);

#ifdef CONFIG_NVRAM_STORAGE_RAMSIM
	memcpy(buf, nvram_mem_buf + addr - NVRAM_MEM_OFFS, size);
	return 0;
#else
	int err;
	int rlen = NVRAM_STORAGE_READ_SEGMENT_SIZE;

	while (size > 0) {
		if (size < NVRAM_STORAGE_READ_SEGMENT_SIZE)
			rlen = size;

		err = flash_read(dev, addr, buf, rlen);
		if (err < 0) {
			SYS_LOG_ERR("read error %d, addr 0x%x, buf %p, size %d", err, addr, buf, size);
			return -EIO;
		}

		addr += rlen;
		buf += rlen;
		size -= rlen;
	}

	return 0;
#endif
}

int nvram_storage_erase(struct device *dev, u32_t addr,
				      s32_t size)
{
	SYS_LOG_INF("erase addr 0x%x size 0x%x", addr, size);

#ifdef CONFIG_NVRAM_STORAGE_RAMSIM
	memset(nvram_mem_buf + addr - NVRAM_MEM_OFFS, 0xff, size);
	return 0;
#else
	return flash_erase(dev, (off_t)addr, (size_t)size);
#endif
}

struct device *nvram_storage_init(void)
{
	struct device *dev;

	SYS_LOG_DBG("init nvram storage");

#ifdef CONFIG_NVRAM_STORAGE_RAMSIM
	SYS_LOG_INF("init simulated nvram storage: addr 0x%x size 0x%x",
		    NVRAM_MEM_OFFS, NVRAM_MEM_SIZE);

	memset(nvram_mem_buf, 0xff, NVRAM_MEM_SIZE);

	/* dummy device pointer */
	dev = (struct device *)-1;
#else
	dev = device_get_binding(CONFIG_NVRAM_STORAGE_DEV_NAME);
	if (!dev) {
		SYS_LOG_ERR("cannot found device \'%s\'\n",
			    CONFIG_NVRAM_STORAGE_DEV_NAME);
		return NULL;
	}
#endif
	return dev;
}

#ifdef CONFIG_NVRAM_USER_STORAGE_EXT_FLASH
struct device *nvram_user_ext_storage_init(void)
{
	struct device *dev;

	SYS_LOG_DBG("init nvram user ext storage");

	dev = device_get_binding(CONFIG_NVRAM_USER_STORAGE_DEV_NAME);
	if (!dev) {
		SYS_LOG_ERR("cannot found device \'%s\'\n",
			    CONFIG_NVRAM_USER_STORAGE_DEV_NAME);
		return NULL;
	}

	return dev;
}
#endif
