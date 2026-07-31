/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file SPI ROM driver interface for Actions SoC
 */

#ifndef _ACTIONS_SOC_ROM_XSPI_H_
#define _ACTIONS_SOC_ROM_XSPI_H_

#ifdef CONFIG_SOC_SERIES_ANDESC
struct spinor_info;
struct spi_info;

struct spimem_operation_api {
	unsigned int (*prepare_op)(struct spi_info *si);
	int (*transfer)(struct spi_info *si, unsigned char cmd, unsigned int addr,
			      int addr_len, void *buf, int length, unsigned char dummy_len, unsigned int flag);
    void (*read_page)(struct spi_info *si, unsigned int addr, int addr_len, void *buf, int len);
	int (*erase_block)(struct spi_info *si, unsigned int addr);
	void (*continuous_read_reset)(struct spi_info *si);
    int (*write_cmd_addr)(struct spi_info *si, unsigned char cmd,
    				    unsigned int addr, int addr_len);
    int (*write_cmd)(struct spi_info *si, unsigned char cmd);
};
#endif

struct xspi_info
{
	/* spi controller address */
	unsigned int base;
	unsigned int flag;

	unsigned short cs_gpio;
	unsigned char bus_width;
	unsigned char delay_chain;

	unsigned int freq_mhz;
	unsigned int dma_base;

	void (*set_cs)(struct xspi_info *sri, int value);
	void (*set_clk)(struct xspi_info *sri, unsigned int freq_khz);
	void (*prepare_hook)(struct xspi_info *sri);
#ifdef CONFIG_SOC_SERIES_ANDESC
	void (*wait_rb_ready)(unsigned int key, struct xspi_info *si);
	void (*sys_irq_lock_cb)(SYS_IRQ_FLAGS *flags);
	void (*sys_irq_unlock_cb)(const SYS_IRQ_FLAGS *flags);

	const struct spimem_operation_api * spimem_op_api;
#endif
};

struct spinor_rom_operation_api {
	int (*init)(struct xspi_info *sri);

	/* 参数与A版兼容，新增加参数为sni以支持多实例 */
	void (*set_clk)(unsigned int clk_mhz, struct spinor_info *sni);

	unsigned int (*read_chipid) (struct xspi_info *sri);

	unsigned int (*read_status) (struct xspi_info *sri, unsigned char cmd);
	void (*write_status)(struct xspi_info *sri, unsigned char cmd,
			unsigned char *status, int len);

	int (*read) (struct xspi_info *sri, unsigned int addr, void *buf, int len);
	int (*write) (struct xspi_info *sri, unsigned int addr, const void *buf, int len);
	int (*erase) (struct xspi_info *sri, unsigned int addr, int len);

#ifdef CONFIG_SOC_SERIES_ANDESC
    /* 保持版本向前兼容，新扩展的函数放到后面 */
	int (*write_rdm) (struct xspi_info *sni, unsigned int addr, const void *buf, int len);
    int (*transfer)(struct xspi_info *sni, unsigned char cmd, unsigned int addr,
                    int addr_len, void *buf, int length, unsigned char dummy_len, unsigned int flag);
    void (*reset_status)(struct xspi_info *sni);
    void (*set_high_performance)(struct xspi_info *sni, uint8_t on);
    void (*deep_sleep)(struct xspi_info *sni, uint8_t on);
#endif
};

typedef int (*rom_spimem_trans_func_t)(struct xspi_info *si,
				   unsigned char cmd, unsigned int addr,
				   int addr_len, void *buf, int length,
				   unsigned char dummy_len, unsigned int flag);

/* flags for spi memory transfer function */
#define XSPI_MEM_TFLAG_MIO_DATA			0x01
#define XSPI_MEM_TFLAG_MIO_ADDR_DATA		0x02
#define XSPI_MEM_TFLAG_MIO_CMD_ADDR_DATA	0x04
#define XSPI_MEM_TFLAG_MIO_MASK			0x07
#define XSPI_MEM_TFLAG_WRITE_DATA		0x08
#define XSPI_MEM_TFLAG_ENABLE_RANDOMIZE		0x10

#define XSPI_ROM_ACTS_API_PTR_ADDR		0x5468
#define XSPI_ROM_ACTS_PREPARE_HOOK_ADDR		0x416a
#define XSPI_ROM_ACTS_API_TRANSFER_ADDR		0x4280

#define XSPI_FLAG_NO_IRQ_LOCK			(0x1 << 0)

#endif	/* _ACTIONS_SOC_ROM_XSPI_H_ */
