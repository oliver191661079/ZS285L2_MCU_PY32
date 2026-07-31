/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file peripheral clock interface for Actions SoC
 */

#include <kernel.h>
#include <device.h>
#include <soc.h>
#include <board.h>
#include <string.h>
#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
#include <soc_rom_xspi.h>
#endif
#include <soc.h>

#define MAX_PSRAM_CLK_FREQ (100)
extern void soc_set_cpu_clk(int dsp_mhz, int cpu_mhz);

#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS

void spi1_dcache_init(void)
{
	sys_write32(sys_read32(CMU_MEMCLKEN) | (1 << 28), CMU_MEMCLKEN);

	acts_clock_peripheral_enable(CLOCK_ID_SPI1);
	acts_reset_peripheral(RESET_ID_SPI1);

	/* init spi1 controller, 4x IO, spi mode 3 */
	sys_write32((1 << 3) | (1 << 4) | (1 << 5) | (3 << 10) |
			(8 << 16) |  (3 << 28), SPI1_CTL);

	acts_clock_peripheral_enable(CLOCK_ID_SPI1_CACHE);
	acts_reset_peripheral(RESET_ID_SPI1_CACHE);

	// spi1 dcache enable
	sys_write32(0x00, SPI1_DCACHE_CTL);

	// spi1 dcache enable
	sys_write32(0x01, SPI1_DCACHE_CTL);

	sys_write32(CONFIG_SOC_MAPPING_PSRAM_ADDR, SPI1_DCACHE_START_ADDR);
	sys_write32(CONFIG_SOC_MAPPING_PSRAM_ADDR + CONFIG_SOC_MAPPING_PSRAM_SIZE,
			SPI1_DCACHE_END_ADDR);

	/* clear mapping */
	sys_write32(0x0, SPI_CACHE_MAPPING_ADDR0 + 7 * 8);
	sys_write32(0x0, SPI_CACHE_ADDR0_ENTRY + 7 * 8);

	/* invalid cache */
	sys_write32((0<<5)|(0x1<<3)|(0x0<<1)|0x1, SPI1_DCACHE_OPERATE);
	while ((sys_read32(SPICACHE_INVALIDATE) & 0x1) == 1)
		;
	//ÉèÖÃcpu¶ËÓ³Éä¹ØÏµ
	sys_write32(0x0, SPI_CACHE_ADDR0_ENTRY + 7 * 8);
	/* default: mapping size: 16MB */
	sys_write32(CONFIG_SOC_MAPPING_PSRAM_ADDR | 0x0f, SPI_CACHE_MAPPING_ADDR0 + 7 * 8);

	//2MÎ»ÖÃ ÉèÖÃdspÓ³Éä¹ØÏµ
	soc_memctrl_set_dsp_mapping(0, 0x90000000 | 0xf, 0x0);


	// set DMA access psram through dcache,±ÜÃâDMAÊý¾ÝºÍCPUÊý¾Ý²»Í¬²½
	sys_write32(sys_read32(SPI1_DCACHE_CTL) | (0x1 << 6), SPI1_DCACHE_CTL);

	// enable lowpower mode
	sys_write32(sys_read32(SPI1_DCACHE_CTL) | (0x1 << 5), SPI1_DCACHE_CTL);
}




void soc_psram_mapping_init(void)
{

	board_init_psram_pins();

#if 1
	/* spi: clock source: ck64m, clock div = 2 */
	pmu_spll_init();
	sys_write32((sys_read32(CMU_SPICLK) & (~0xff00)) | (3 << 14) | (0 << 8), CMU_SPICLK);

#else
	/* spi: clock source: corepll, clock div = 2 */
	soc_freq_set_cpu_clk(CONFIG_SOC_DSP_CLK_FREQ / 1000, CONFIG_SOC_CPU_CLK_FREQ / 1000);

	{
		int coreclk = (sys_read32(CORE_PLL_CTL) & 0x7f) * 6;
		int clkdiv = (coreclk + MAX_PSRAM_CLK_FREQ - 1) / MAX_PSRAM_CLK_FREQ - 1;
		if (clkdiv < 0)
			clkdiv = 0;

		sys_write32((sys_read32(CMU_SPICLK) & (~0xff00)) | (2 << 14) | (clkdiv << 8), CMU_SPICLK);
	}
#endif

	spi1_dcache_init();

#if 0
	sys_write32(0, SPI1_DCACHE_UNCACHE_ADDR_START);
	sys_write32(0x500000, SPI1_DCACHE_UNCACHE_ADDR_END);
	sys_write32(sys_read32(SPI1_DCACHE_CTL) | 0x04, SPI1_DCACHE_CTL);
#endif
}
#else
void soc_psram_cache_init(void)
{
	/* spi: clock source: corepll, clock div = 2 */
	soc_freq_set_cpu_clk(CONFIG_SOC_DSP_CLK_FREQ / 1000, CONFIG_SOC_CPU_CLK_FREQ / 1000, CONFIG_SOC_CPU_CLK_FREQ / 2000);

	/* use psram dcache as sram */
	sys_write32(sys_read32(CMU_MEMCLKEN) | (1 << 28), CMU_MEMCLKEN);

	/* mapping psram dcache address to 0x68000000 */
	soc_memctrl_set_mapping(7, 0x68000000, 0x0);

	/* clear dcache ram */
	ROM_MEMSET((void *)0x68000000, 0x0, 0x8000);
}
#endif