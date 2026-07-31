/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief SOC utils interface
 */

#ifndef __SOC_UTILS_H__
#define __SOC_UTILS_H__

u32_t utils_crc32(u32_t crc, const u8_t *buf, int len);

/* clear soc watchdog */
static inline void soc_watchdog_clear(void)
{
	sys_write32(sys_read32(WD_CTL) | 0x1, WD_CTL);
}

static inline void soc_watchdog_stop(void)
{
	sys_write32(sys_read32(WD_CTL) & ~(1 << 4), WD_CTL);
}

#define API_ROM_MEMSET 		   1
#define API_ROM_MEMCPY 		   2

#define ROM_EXPORT_APIS_ENTRY_ADDR(x)    (CONFIG_ROM_TABLE_ENTRY_ADDR + (4 * (x)))
#define ENTRY_ROM_MEMSET          (*(u32_t*)(ROM_EXPORT_APIS_ENTRY_ADDR(API_ROM_MEMSET)))
#define ENTRY_ROM_MEMCPY          (*(u32_t*)(ROM_EXPORT_APIS_ENTRY_ADDR(API_ROM_MEMCPY)))

#define ROM_MEMSET(a,b,c)         ((void*(*)(void* , int , u32_t ))ENTRY_ROM_MEMSET)((a), (b), (c))
#define ROM_MEMCPY(a,b,c)         ((void*(*)(void* , const void* , u32_t ))ENTRY_ROM_MEMCPY)((a), (b), (c))

#endif /* __SOC_UTILS_H__ */
