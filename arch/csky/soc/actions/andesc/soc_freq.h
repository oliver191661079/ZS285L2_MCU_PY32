/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef	_ACTIONS_SOC_FREQ_H_
#define	_ACTIONS_SOC_FREQ_H_

#ifndef _ASMLANGUAGE

// 8*128fs = 8*128*192K
#define SPDIF_RX_COREPLL_MHZ		(198)

/**
**  sample rate enum
**/
typedef enum {
	SAMPLE_NULL   = 0,
	SAMPLE_8KHZ   = 8,
	SAMPLE_11KHZ  = 11,   /* 11.025khz */
	SAMPLE_12KHZ  = 12,
	SAMPLE_16KHZ  = 16,
	SAMPLE_22KHZ  = 22,   /* 22.05khz */
	SAMPLE_24KHZ  = 24,
	SAMPLE_32KHZ  = 32,
	SAMPLE_44KHZ  = 44,   /* 44.1khz */
	SAMPLE_48KHZ  = 48,
	SAMPLE_64KHZ  = 64,
	SAMPLE_88KHZ  = 88,   /* 88.2khz */
	SAMPLE_96KHZ  = 96,
	SAMPLE_176KHZ = 176,  /* 176.4khz */
	SAMPLE_192KHZ = 192,
} audio_sample_rate_e;

unsigned int soc_freq_get_corepll_freq(void);
unsigned int soc_freq_get_core_freq(void);
unsigned int soc_freq_get_cpu_freq(void);
unsigned int soc_freq_get_dsp_freq(void);

void soc_freq_set_cpu_clk(unsigned int dsp_mhz, unsigned int cpu_mhz, unsigned int ahb_mhz);

void soc_freq_set_dsp_half_and_cpu_the_same(void);
void soc_freq_set_dsp_cpu_only_by_div(unsigned int dsp_mhz, unsigned int cpu_mhz, unsigned int ahb_mhz);

void soc_freq_set_spi_clk(int spi_id, unsigned int spi_freq);
void soc_freq_set_spi_clk_with_corepll(int spi_id, unsigned int spi_freq,
				       unsigned int corepll_freq);

unsigned int soc_freq_set_asrc_freq(u32_t asrc_freq);

unsigned int soc_freq_get_spdif_corepll_freq(u32_t sample_rate);

unsigned int soc_freq_set_spdif_freq(u32_t sample_rate);

#ifdef CONFIG_SOC_SD_USE_COREPLL
void soc_freq_set_sd_clk_with_corepll(int sd_id, unsigned int sd_freq,
	unsigned int corepll_freq);
#endif


#endif /* _ASMLANGUAGE */

#endif /* _ACTIONS_SOC_FREQ_H_	*/
