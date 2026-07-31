/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief common code for SPI Flash (NOR & NAND & PSRAM)
 */

#include <errno.h>
#include <kernel.h>
#include <init.h>
#include <string.h>
#include <linker/sections.h>
#include <flash.h>
#include <soc.h>
#include <misc/byteorder.h>
#include "xspi_mem_acts.h"

#define	XSPI_MEM_CMD_READ_CHIPID		0x9f	/* JEDEC ID */

#define	XSPI_MEM_CMD_FAST_READ			0x0b	/* fast read */
#define	XSPI_MEM_CMD_FAST_READ_X2		0x3b	/* data x2 */
#define	XSPI_MEM_CMD_FAST_READ_X4		0x6b	/* data x4 */
#define	XSPI_MEM_CMD_FAST_READ_X2IO		0xbb	/* addr & data x2 */
#define	XSPI_MEM_CMD_FAST_READ_X4IO		0xeb	/* addr & data x4 */

#define	XSPI_MEM_CMD_ENABLE_WRITE		0x06	/* enable write */
#define	XSPI_MEM_CMD_DISABLE_WRITE		0x04	/* disable write */

#define	XSPI_MEM_CMD_WRITE_PAGE			0x02	/* write one page */
#define XSPI_MEM_CMD_ERASE_BLOCK		0xd8	/* 64KB erase */
#define XSPI_MEM_CMD_CONTINUOUS_READ_RESET	0xff	/* exit quad continuous_read mode */

static xspi_mem_prepare_hook_t prepare_hook;

__XSPI_RAMFUNC static void _xspi_mem_read_data(struct xspi_info *si, void *buf, s32_t len)
{
	if ((len > 16) && (si->dma_base != 0)) {
		xspi_read_data_by_dma(si, (unsigned char *)buf, len);
	} else {
		xspi_read_data(si, (unsigned char *)buf, len);
	}
}

__XSPI_RAMFUNC static void _xspi_mem_write_data(struct xspi_info *si, const void *buf, s32_t len)
{
	if ((len > 16) && (si->dma_base != 0)) {
		xspi_write_data_by_dma(si, (const unsigned char *)buf, len);
	} else {
		xspi_write_data(si, (const unsigned char *)buf, len);
	}
}

ALWAYS_INLINE static void _xspi_mem_write_byte(struct xspi_info *si, u8_t byte)
{
	_xspi_mem_write_data(si, &byte, 1);
}

__XSPI_RAMFUNC void xspi_mem_set_cs(struct xspi_info *si, int value)
{
	xspi_set_cs(si, value);
}

__XSPI_RAMFUNC void xspi_mem_continuous_read_reset(struct xspi_info *si)
{
	xspi_mem_set_cs(si, 0);
	_xspi_mem_write_byte(si, XSPI_MEM_CMD_CONTINUOUS_READ_RESET);
	_xspi_mem_write_byte(si, XSPI_MEM_CMD_CONTINUOUS_READ_RESET);
	xspi_mem_set_cs(si, 1);
}

__XSPI_RAMFUNC void xspi_mem_prepare_hook_install(xspi_mem_prepare_hook_t hook)
{
	prepare_hook = hook;
}

__XSPI_RAMFUNC u32_t xspi_mem_prepare_op(struct xspi_info *si)
{
	u32_t orig_spi_ctl;
	unsigned int use_3wire;

	/* backup old XSPI_CTL */
	orig_spi_ctl = xspi_read(si, XSPI_CTL);
	use_3wire = orig_spi_ctl & XSPI_CTL_SPI_3WIRE;

	/* wait AHB ready */
//	while ((xspi_read(si, XSPI_STATUS) & XSPI_STATUS_READY) == 0)
//		;

	xspi_reset(si);

	if (xspi_controller_id(si) == 0) {
		xspi_write(si, XSPI_CTL,
			  XSPI_CTL_AHB_REQ | XSPI_CTL_IO_MODE_1X | XSPI_CTL_WR_MODE_DISABLE |
			  XSPI_CTL_RX_FIFO_EN | XSPI_CTL_TX_FIFO_EN | XSPI_CTL_WR_MODE_DISABLE |
			  XSPI_CTL_SS | (0x8 << XSPI_CTL_DELAYCHAIN_SHIFT));
	} else {
		xspi_write(si, XSPI_CTL,
			  XSPI1_CTL_AHB_REQ | XSPI_CTL_IO_MODE_1X | XSPI_CTL_WR_MODE_DISABLE |
			  XSPI_CTL_RX_FIFO_EN | XSPI_CTL_TX_FIFO_EN | XSPI_CTL_WR_MODE_DISABLE |
			  XSPI_CTL_SS | (0x8 << XSPI_CTL_DELAYCHAIN_SHIFT));
	}

	if (use_3wire)
		xspi_set_bits(si, XSPI_CTL, XSPI_CTL_SPI_3WIRE, XSPI_CTL_SPI_3WIRE);

	if (si->delay_chain != 0xff)
		xspi_set_bits(si, XSPI_CTL, XSPI_CTL_DELAYCHAIN_MASK,
			     si->delay_chain << XSPI_CTL_DELAYCHAIN_SHIFT);

	if (si->flag & XSPI_FLAG_SPI_MODE0)
		xspi_set_bits(si, XSPI_CTL, XSPI_CTL_MODE_MASK, XSPI_CTL_MODE_MODE0);

	if (si->set_clk && si->freq_mhz)
		si->set_clk(si, si->freq_mhz);

	if (prepare_hook)
		prepare_hook(si);

	return orig_spi_ctl;
}

__XSPI_RAMFUNC void xspi_mem_complete_op(struct xspi_info *si, u32_t orig_spi_ctl)
{
	/* restore old XSPI_CTL */
	xspi_write(si, XSPI_CTL, orig_spi_ctl);
	xspi_delay();
}

__XSPI_RAMFUNC int xspi_mem_transfer(struct xspi_info *si, u8_t cmd, u32_t addr,
			      s32_t addr_len, void *buf, s32_t length,
			      u8_t dummy_len, u32_t flag)
{

	u32_t orig_spi_ctl, i, addr_be;
	u32_t key = 0;

	/* address to big endian */
	if (addr_len > 0)
		sys_memcpy_swap(&addr_be, &addr, addr_len);

	if (xspi_need_lock_irq(si)) {
		key = irq_lock();
	}

	orig_spi_ctl = xspi_mem_prepare_op(si);

	xspi_mem_set_cs(si, 0);

	/* cmd & address & data all use multi io mode */
	if (flag & XSPI_MEM_TFLAG_MIO_CMD_ADDR_DATA)
		xspi_setup_bus_width(si, si->bus_width);

	/* write command */
	_xspi_mem_write_byte(si, cmd);

	/* address & data use multi io mode */
	if (flag & XSPI_MEM_TFLAG_MIO_ADDR_DATA)
		xspi_setup_bus_width(si, si->bus_width);

	if (addr_len > 0)
		_xspi_mem_write_data(si, &addr_be, addr_len);

	/* send dummy bytes */
	for (i = 0; i < dummy_len; i++)
		_xspi_mem_write_byte(si, 0);

	/* only data use multi io mode */
	if (flag & XSPI_MEM_TFLAG_MIO_DATA)
		xspi_setup_bus_width(si, si->bus_width);

	/* send or get data */
	if (length > 0) {
		if (flag & XSPI_MEM_TFLAG_WRITE_DATA) {
			_xspi_mem_write_data(si, buf, length);
		} else {
			_xspi_mem_read_data(si, buf, length);
		}
	}

	/* restore spi bus width to 1-bit */
	if (flag & XSPI_MEM_TFLAG_MIO_MASK)
		xspi_setup_bus_width(si, 1);

	xspi_mem_set_cs(si, 1);

	xspi_mem_complete_op(si, orig_spi_ctl);

	if (xspi_need_lock_irq(si)) {
		irq_unlock(key);
	}

	return 0;
}

__XSPI_RAMFUNC int xspi_mem_write_cmd_addr(struct xspi_info *si, u8_t cmd,
				    u32_t addr, s32_t addr_len)
{
	return xspi_mem_transfer(si, cmd, addr, addr_len, NULL, 0, 0, 0);
}

__XSPI_RAMFUNC int xspi_mem_write_cmd(struct xspi_info *si, u8_t cmd)
{
	return xspi_mem_write_cmd_addr(si, cmd, 0, 0);
}

void xspi_mem_read_chipid(struct xspi_info *si, void *chipid, s32_t len)
{
	xspi_mem_transfer(si, XSPI_MEM_CMD_READ_CHIPID, 0, 0, chipid, len, 0, 0);
}

__XSPI_RAMFUNC u8_t xspi_mem_read_status(struct xspi_info *si, u8_t cmd)
{
	u32_t status;

	xspi_mem_transfer(si, cmd, 0, 0, &status, 1, 0, 0);

	return status;
}

__XSPI_RAMFUNC void xspi_mem_set_write_protect(struct xspi_info *si, bool protect)
{
	if (protect)
		xspi_mem_write_cmd(si, XSPI_MEM_CMD_DISABLE_WRITE);
	else
		xspi_mem_write_cmd(si, XSPI_MEM_CMD_ENABLE_WRITE);
}

__XSPI_RAMFUNC void xspi_mem_read_page(struct xspi_info *si,
		u32_t addr, s32_t addr_len,
		void *buf, s32_t len)
{
	u8_t cmd;
	u32_t flag;

	if (si->bus_width == 4) {
		cmd = XSPI_MEM_CMD_FAST_READ_X4;
		flag = XSPI_MEM_TFLAG_MIO_DATA;
	} else if (si->bus_width == 2) {
		cmd = XSPI_MEM_CMD_FAST_READ_X2;
		flag = XSPI_MEM_TFLAG_MIO_DATA;
	} else {
		cmd = XSPI_MEM_CMD_FAST_READ;
		flag = 0;
	}

	xspi_mem_transfer(si, cmd, addr, addr_len, buf, len, 1, flag);
}

__XSPI_RAMFUNC int xspi_mem_erase_block(struct xspi_info *si, u32_t addr)
{
	xspi_mem_set_write_protect(si, false);
	xspi_mem_write_cmd_addr(si, XSPI_MEM_CMD_ERASE_BLOCK, addr, 3);

	return 0;
}
