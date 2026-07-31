/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief common code for SPI memory (NOR & NAND & PSRAM)
 */

#ifndef __XSPI_MEM_ACTS_H__
#define __XSPI_MEM_ACTS_H__

#include <kernel.h>
#include "xspi_acts.h"

/* no need to use ramfunc if nor is connected by spi1 */
#if defined(CONFIG_XSPI_NOR_ROM_ACTS) && defined(CONFIG_XSPI1_NOR_ACTS)
#define __XSPI_RAMFUNC
#else
#define __XSPI_RAMFUNC __ramfunc
#endif

#define XSPI_MEM_TFLAG_MIO_DATA			0x01
#define XSPI_MEM_TFLAG_MIO_ADDR_DATA		0x02
#define XSPI_MEM_TFLAG_MIO_CMD_ADDR_DATA	0x04
#define XSPI_MEM_TFLAG_MIO_MASK			0x07
#define XSPI_MEM_TFLAG_WRITE_DATA		0x08
#define XSPI_MEM_TFLAG_ENABLE_RANDOMIZE		0x10

void xspi_mem_read_chipid(struct xspi_info *si, void *chipid, s32_t len);
u8_t xspi_mem_read_status(struct xspi_info *si, u8_t cmd);

void xspi_mem_read_page(struct xspi_info *si,
		u32_t addr, s32_t addr_len,
		void *buf, s32_t len);
void xspi_mem_continuous_read_reset(struct xspi_info *si);

void xspi_mem_set_write_protect(struct xspi_info *si, bool protect);
int xspi_mem_erase_block(struct xspi_info *si, u32_t page);

typedef void (*xspi_mem_prepare_hook_t) (struct xspi_info *si);
void xspi_mem_prepare_hook_install(xspi_mem_prepare_hook_t hook);

int xspi_mem_transfer(struct xspi_info *si, u8_t cmd, u32_t addr,
		    s32_t addr_len, void *buf, s32_t length,
		    u8_t dummy_len, u32_t flag);

int xspi_mem_write_cmd_addr(struct xspi_info *si, u8_t cmd,
			  u32_t addr, s32_t addr_len);

int xspi_mem_write_cmd(struct xspi_info *si, u8_t cmd);

#endif	/* __XSPI_MEM_ACTS_H__ */
