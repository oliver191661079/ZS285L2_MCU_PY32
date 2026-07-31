/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-13-上午10:52:46             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_cache.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-13-上午10:52:46
 *******************************************************************************/

#ifndef SOC_CACHE_H_
#define SOC_CACHE_H_

#define     SPICACHE_CTL_LOW_POWER_EN                                         5
#define     SPICACHE_CTL_PROF_EN                                              4
#define     SPICACHE_CTL_ABORT_EN                                             3
#define     SPICACHE_CTL_UNCACHE_EN                                           2
#define     SPICACHE_CTL_PREF_EN                                              1
#define     SPICACHE_CTL_EN                                                   0

#define     SPICACHE_CTL                                                      (SPICACHE_REG_BASE+0x0000)
#define     SPICACHE_INVALIDATE                                               (SPICACHE_REG_BASE+0x0004)
#define     SPICACHE_UNCACHE_ADDR_START                                       (SPICACHE_REG_BASE+0x0008)
#define     SPICACHE_UNCACHE_ADDR_END                                         (SPICACHE_REG_BASE+0x000C)
#define     SPICACHE_TOTAL_MISS_COUNT                                         (SPICACHE_REG_BASE+0x0010)
#define     SPICACHE_TOTAL_HIT_COUNT                                          (SPICACHE_REG_BASE+0x0014)
#define     SPICACHE_PROFILE_INDEX_START                                      (SPICACHE_REG_BASE+0x0018)
#define     SPICACHE_PROFILE_INDEX_END                                        (SPICACHE_REG_BASE+0x001C)
#define     SPICACHE_RANGE_INDEX_MISS_COUNT                                   (SPICACHE_REG_BASE+0x0020)
#define     SPICACHE_RANGE_INDEX_HIT_COUNT                                    (SPICACHE_REG_BASE+0x0024)
#define     SPICACHE_PROFILE_ADDR_START                                       (SPICACHE_REG_BASE+0x0028)
#define     SPICACHE_PROFILE_ADDR_END                                         (SPICACHE_REG_BASE+0x002C)
#define     SPICACHE_RANGE_ADDR_MISS_COUNT                                    (SPICACHE_REG_BASE+0x0030)
#define     SPICACHE_RANGE_ADDR_HIT_COUNT                                     (SPICACHE_REG_BASE+0x0034)

#define     SPI1_DCACHE_CTL                                                   (SPI1_DCACHE_REG_BASE+0x0000)
#define     SPI1_DCACHE_OPERATE                                               (SPI1_DCACHE_REG_BASE+0x0004)
#define     SPI1_DCACHE_UNCACHE_ADDR_START                                    (SPI1_DCACHE_REG_BASE+0x0008)
#define     SPI1_DCACHE_UNCACHE_ADDR_END                                      (SPI1_DCACHE_REG_BASE+0x000C)
#define     SPI1_DCACHE_CK802_MISS_COUNT                                      (SPI1_DCACHE_REG_BASE+0x0010)
#define     SPI1_DCACHE_CK802_HIT_COUNT                                       (SPI1_DCACHE_REG_BASE+0x0014)
#define     SPI1_DCACHE_CK802_WRITEBACK_COUNT                                 (SPI1_DCACHE_REG_BASE+0x0018)
#define     SPI1_DCACHE_DSP_MISS_COUNT                                        (SPI1_DCACHE_REG_BASE+0x001c)
#define     SPI1_DCACHE_DSP_HIT_COUNT                                         (SPI1_DCACHE_REG_BASE+0x0020)
#define     SPI1_DCACHE_DSP_WRITEBACK_COUNT                                   (SPI1_DCACHE_REG_BASE+0x0024)
#define     SPI1_DCACHE_DMA_MISS_COUNT                                        (SPI1_DCACHE_REG_BASE+0x0028)
#define     SPI1_DCACHE_DMA_HIT_COUNT                                         (SPI1_DCACHE_REG_BASE+0x002c)
#define     SPI1_DCACHE_DMA_WRITEBACK_COUNT                                   (SPI1_DCACHE_REG_BASE+0x0030)
#define     SPI1_DCACHE_M4K_MISS_COUNT                                        (SPI1_DCACHE_REG_BASE+0x0034)
#define     SPI1_DCACHE_M4K_HIT_COUNT                                         (SPI1_DCACHE_REG_BASE+0x0038)
#define     SPI1_DCACHE_M4K_WRITEBACK_COUNT                                   (SPI1_DCACHE_REG_BASE+0x003c)


#endif /* SOC_CACHE_H_ */
