/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file clock frequence interface
 */

#include <kernel.h>
#include <init.h>
#include <device.h>
#include <soc.h>
#include <soc_freq.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SOC_FREQ

unsigned int soc_freq_get_corepll_freq(void)
{
	unsigned int val;

	val = sys_read32(CORE_PLL_CTL);

	if (!(val & (1 << 7)))
		return 0;

	return ((val & 0x7f) * 6);
}

static unsigned int soc_freq_get_spll_freq(void)
{
    return 64;
}

unsigned int soc_freq_get_core_freq(void)
{
	unsigned int freq;

	freq = soc_freq_get_corepll_freq();

	if(!freq){
        return 0;
	}else{
        freq = freq >> ((sys_read32(CMU_SYSCLK) >> 4) & 0x3);
	}

	return freq;
}

unsigned int soc_freq_get_cpu_freq(void)
{
	unsigned int freq, core_freq;

	core_freq = soc_freq_get_core_freq();

	freq = core_freq >> ((sys_read32(CMU_SYSCLK) >> 20) & 0x3);

	freq = freq * (((sys_read32(CMU_SYSCLK) >> 8) & 0xf) + 1) / 16;

	return freq;
}

unsigned int soc_freq_get_dsp_freq(void)
{
	return soc_freq_get_core_freq();
}


/* delay 100us when cpu clock is 24M HOSC */
static void delay_100us(void)
{
	volatile int i;

	for (i = 0; i < 150; i++)
		;
}

#ifdef CONFIG_SOC_SPREAD_SPECTRUM
static void enable_spread_spectrum(void)
{
	//u32_t reg_val = sys_read32(CORE_PLL_DEBUG);

	//reg_val = reg_val & 0xffff8000;
	//reg_val = reg_val | 0x00005f92;

	/*SSC_EN,SSC_DIV=1/2,HNUM=113,PSTEP=4/4FSTEP=9,second order*/
	sys_write32(0x4ce0599e, CORE_PLL_DEBUG);

	//sys_write32(sys_read32(CORE_PLL_DEBUG) & 0xffff8000, CORE_PLL_DEBUG);
	//sys_write32(sys_read32(CORE_PLL_DEBUG) | 0x00005f92, CORE_PLL_DEBUG);
}

static void disable_spread_spectrum(void)
{
	sys_write32(sys_read32(CORE_PLL_DEBUG) & ~(1 << 14), CORE_PLL_DEBUG);
}
#endif

void soc_freq_set_dsp_half_and_cpu_the_same(void)
{
	unsigned int flags;

	flags = irq_lock();
	unsigned int reg = sys_read32(CMU_SYSCLK) & (~0xf00);
	reg &= (~0x30000);
	reg |= 1 << 16;
	reg |= 0x7f << 8;
	sys_write32(reg,CMU_SYSCLK);

	irq_unlock(flags);

}
void soc_freq_set_dsp_cpu_only_by_div(unsigned int dsp_mhz, unsigned int cpu_mhz, unsigned int ahb_mhz)
{
	unsigned int flags;
	unsigned int val;
	unsigned int corepll;
	unsigned int real_cpu_mhz;

	flags = irq_lock();
	corepll = (sys_read32(CMU_COREPLL_CTL)&(0x7f));
	irq_unlock(flags);

	corepll *= 6;

	if (dsp_mhz > corepll) {
		SYS_LOG_ERR("dsp freq:%d is larger than corepll:%d", dsp_mhz, corepll);
		return;
	}

	unsigned int coreclk_div = 0;
	for(unsigned int i = 0; i < 4; i++){
		unsigned int clk = corepll<<i;
		if(clk <= dsp_mhz){
			coreclk_div = i;
			break;
		}
	}

	flags = irq_lock();
	val = sys_read32(CMU_SYSCLK);
	val &= (~(0xf << 8));
	val |= ((((cpu_mhz << 4) / dsp_mhz) - 1) << 8);

	real_cpu_mhz = ((cpu_mhz << 4) / dsp_mhz) * dsp_mhz / 16;
	if ((real_cpu_mhz / ahb_mhz) <= 2)
		val &= ~(1 << 12); /* SCLK divisor 2 */
	else
		val |= (1 << 12); /* SCLK divisor 4 */

	val &= (~(0x3 << 4));
	val |= (coreclk_div&0x3)<<4;
	sys_write32(val, CMU_SYSCLK);
	irq_unlock(flags);

}

void soc_freq_set_cpu_clk(unsigned int dsp_mhz, unsigned int cpu_mhz, unsigned int ahb_mhz)
{
	unsigned int val;
	unsigned int flags;
	unsigned int real_cpu_mhz;

	flags = irq_lock();

	/* switch to hosc */
	sys_write32((sys_read32(CMU_SYSCLK) & ~0x7) | 0x3, CMU_SYSCLK);

	/* wait switch over */
	while (!((sys_read32(CMU_SYSCLK) & 0x7) == 0x3))
	;

#ifdef CONFIG_SOC_SPREAD_SPECTRUM
	disable_spread_spectrum();
#endif

	//0x80 enable corepll
	val = ((dsp_mhz * 4) / 24) | 0x80;
	sys_write32(val, CMU_COREPLL_CTL);

	delay_100us();

	val = sys_read32(CMU_SYSCLK);

	val &= (~(0xf << 8));
	val |= ((((cpu_mhz << 4) / dsp_mhz) - 1) << 8);
	val |= (1 << 16);

	real_cpu_mhz = ((cpu_mhz << 4) / dsp_mhz) * dsp_mhz / 16;
	if ((real_cpu_mhz / ahb_mhz) <= 2)
		val &= ~(1 << 12); /* SCLK divisor 2 */
	else
		val |= (1 << 12); /* SCLK divisor 4 */

	sys_write32(val, CMU_SYSCLK);

#ifdef CONFIG_SOC_SPREAD_SPECTRUM
	enable_spread_spectrum();
#endif

	/* swich cpu clock to corepll, ahb_clk = cpu_clk / 2 */
	sys_write32((sys_read32(CMU_SYSCLK) & ~0x7) | 0x2, CMU_SYSCLK);

	/* wait switch over */
	while (!((sys_read32(CMU_SYSCLK) & 0x7) == 0x2))
		;

    irq_unlock(flags);
}

static unsigned int calc_spi_clk_div(unsigned int corepll_freq, unsigned int spi_freq)
{
	int sel;

	if (corepll_freq > spi_freq && corepll_freq <= (spi_freq * 3 / 2)) {
		/* /1.5 */
		sel = 30;
	} else if ((corepll_freq > 2 * spi_freq) && corepll_freq <= (spi_freq * 5 / 2)) {
		/* /2.5 */
		sel = 31;
	} else {
		/* /n */
		sel = (corepll_freq + spi_freq - 1) / spi_freq - 1;
		if (sel > 29) {
			sel = 29;
		}
	}

	return sel;
}

#ifdef CONFIG_SOC_SD_USE_COREPLL
void soc_freq_set_sd_clk_with_corepll(int sd_id, unsigned int sd_freq,
	unsigned int corepll_freq)
{
	unsigned int key;
	u8_t sel;

	sel = (corepll_freq + sd_freq - 1) / sd_freq - 1;
	if (sel > 15)
		sel = 15;

	key = irq_lock();
	if (sd_id == 0) {
		sys_write32((sys_read32(CMU_SDCLK) & ~0xff) | (1 << 6) | (sel << 0), CMU_SDCLK);
	} else {
		sys_write32((sys_read32(CMU_SDCLK) & ~0xff00) | (1 << 14) | (sel << 8), CMU_SDCLK);
	}
	irq_unlock(key);
}
#endif

void soc_freq_set_spi_clk_with_corepll(int spi_id, unsigned int spi_freq,
	unsigned int corepll_freq)
{
#ifdef CONFIG_SOC_SPI_USE_COREPLL
	unsigned int sel, reg_val, key;
	unsigned int reg_shift;

	reg_shift = spi_id * 8;

	sel = calc_spi_clk_div(corepll_freq, spi_freq) & 0x1f;

	key = irq_lock();

	reg_val = sys_read32(CMU_SPICLK);
	reg_val &= ~(0xff << reg_shift);
	reg_val |= (0x80 | sel) << reg_shift;
	sys_write32(reg_val, CMU_SPICLK);

	irq_unlock(key);
#endif
}

void soc_freq_set_spi_clk_with_spll(int spi_id, unsigned int spi_freq,
	unsigned int spll_freq)
{
#ifdef CONFIG_SOC_SPI_USE_SPLL
	unsigned int sel, reg_val, key;
	unsigned int reg_shift;

	reg_shift = spi_id * 8;

	sel = calc_spi_clk_div(spll_freq, spi_freq) & 0x1f;

	key = irq_lock();

	reg_val = sys_read32(CMU_SPICLK);
	reg_val &= ~(0xff << reg_shift);
	reg_val |= (0xc0 | sel) << reg_shift;
	sys_write32(reg_val, CMU_SPICLK);

	irq_unlock(key);
#endif
}


void soc_freq_set_spi_clk(int spi_id, unsigned int spi_freq)
{
#ifdef CONFIG_SOC_SPI_USE_COREPLL
	soc_freq_set_spi_clk_with_corepll(spi_id, spi_freq, soc_freq_get_corepll_freq());
#endif

#ifdef CONFIG_SOC_SPI_USE_SPLL
    soc_freq_set_spi_clk_with_spll(spi_id, spi_freq, soc_freq_get_spll_freq());
#endif
}


unsigned int soc_freq_set_asrc_freq(u32_t asrc_freq)
{
    unsigned int asrcclk_div;

    u32_t core_freq = soc_freq_get_core_freq();

    //fix use corepll as clock source
    asrcclk_div = (core_freq / asrc_freq);
    asrcclk_div <<= 1;

    if (asrcclk_div > 6) {
        asrcclk_div = 5;
    } else if (asrcclk_div == 6) {
        asrcclk_div = 4;
    } else if (asrcclk_div == 5) {
        asrcclk_div = 3;
    } else if (asrcclk_div == 4) {
        asrcclk_div = 2;
    } else if (asrcclk_div == 3) {
        asrcclk_div = 1;
    } else {
        asrcclk_div = 0;
    }

    asrcclk_div |= 0x10;

    sys_write32(asrcclk_div, CMU_ASRCCLK);

    return 0;
}

unsigned int soc_freq_get_spdif_corepll_freq(u32_t sample_rate)
{
	u32_t core_pll_value = 0;

	switch(sample_rate) {
		case SAMPLE_192KHZ:
		case SAMPLE_176KHZ:
			// corepll 198MHz,  rxclk  198MHz
			core_pll_value = SPDIF_RX_COREPLL_MHZ;
			break;

		case SAMPLE_96KHZ:
			core_pll_value = 132;
			break;

		case SAMPLE_88KHZ:
		case SAMPLE_64KHZ:
		case SAMPLE_48KHZ:
		case SAMPLE_44KHZ:
		case SAMPLE_32KHZ:
		case SAMPLE_24KHZ:
		case SAMPLE_22KHZ:
			// corepll 120MHz,  , rxclk  120/1= 120MHz
			core_pll_value = 120;
			break;

		case SAMPLE_16KHZ:
			// corepll 48MHz,  , rxclk  48MHz
			core_pll_value = 48;
			break;

		case SAMPLE_12KHZ:
		case SAMPLE_11KHZ:
			// corepll 36MHz,  , rxclk  36MHz
			core_pll_value = 36;
			break;

		default:
			break;
	}

    return core_pll_value;
}

unsigned int soc_freq_set_spdif_freq(u32_t sample_rate)
{
	u8_t  spdif_rx_clk_div = 0;
	u32_t core_pll_value;
	u32_t rxclk = 0;

	core_pll_value = soc_freq_get_spdif_corepll_freq(sample_rate);

	switch(sample_rate) {
		case SAMPLE_192KHZ:
		case SAMPLE_176KHZ:
			// corepll 198MHz,  rxclk  198MHz
			rxclk = core_pll_value;
			spdif_rx_clk_div = 0;
			break;

		case SAMPLE_96KHZ:
		case SAMPLE_88KHZ:
		case SAMPLE_64KHZ:
		case SAMPLE_48KHZ:
		case SAMPLE_44KHZ:
		case SAMPLE_32KHZ:
		case SAMPLE_24KHZ:
		case SAMPLE_22KHZ:
			// corepll 120MHZ,  , rxclk  120/1= 120MHz
			rxclk = core_pll_value;
			spdif_rx_clk_div = 0;
			break;

		case SAMPLE_16KHZ:
			// corepll 48MHz,  , rxclk  48MHz
			rxclk = core_pll_value;
			spdif_rx_clk_div = 0;
			break;

		case SAMPLE_12KHZ:
		case SAMPLE_11KHZ:
			// corepll 36MHz,  , rxclk  36MHz
			rxclk = core_pll_value;
			spdif_rx_clk_div = 0;
			break;

		default:
			printk("%d, not support !\n", sample_rate);
			break;
	}

    if(core_pll_value){
        sys_write32(((sys_read32(CMU_SPDIFCLK) & (~CMU_SPDIFCLK_SPDIFRXCLKDIV_MASK)) \
            | (spdif_rx_clk_div << CMU_SPDIFCLK_SPDIFRXCLKDIV_SHIFT)), CMU_SPDIFCLK);
    }

	return rxclk;
}
