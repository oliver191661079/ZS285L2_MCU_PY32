/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief NVRAM storage interface
 */

#ifndef __NVRAM_STORAGE_H__
#define __NVRAM_STORAGE_H__

#include <kernel.h>
#include <device.h>

extern struct device *nvram_storage_init(void);
extern int nvram_storage_read(struct device *dev, uint32_t addr,
			      void *buf, int32_t size);
extern int nvram_storage_write(struct device *dev, uint32_t addr,
			       const void *buf, int32_t size);
extern int nvram_storage_erase(struct device *dev, uint32_t addr, int32_t size);

#ifdef CONFIG_NVRAM_USER_STORAGE_EXT_FLASH
extern struct device *nvram_user_ext_storage_init(void);
#endif

#endif/* #define __NVRAM_STORAGE_H__ */
