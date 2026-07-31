/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Audio input driver for Actions SoC
 */

#include <kernel.h>
#include <init.h>
#include <device.h>
#include <irq.h>
#include <dma.h>
#include <soc.h>
#include <string.h>
#include <audio.h>
#include <atomic.h>

#define SYS_LOG_DOMAIN "audio_in"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_AUDIO_IN_ACTS

/* register defines */

/* ADC_DIGCTL register */
#define ADC_DIGCTL_DMICGAIN_SHIFT	0
#define ADC_DIGCTL_DMICGAIN(x)		(((x) & 0x3) << ADC_DIGCTL_DMICGAIN_SHIFT)
#define ADC_DIGCTL_DMICGAIN_MASK	ADC_DIGCTL_DMICGAIN(0x3)
#define ADC_DIGCTL_AADEN		BIT(2)
#define ADC_DIGCTL_ADDEN		BIT(3)
#define ADC_DIGCTL_DRFS			BIT(5)
#define ADC_DIGCTL_HPFREN		BIT(6)
#define ADC_DIGCTL_HPFLEN		BIT(7)
#define ADC_DIGCTL_DMREN		BIT(8)
#define ADC_DIGCTL_DMLEN		BIT(9)
#define ADC_DIGCTL_DMOSRS		BIT(10)
#define ADC_DIGCTL_ADCGC_SHIFT		11
#define ADC_DIGCTL_ADCGC(x)		(((x) & 0xf) << ADC_DIGCTL_ADCGC_SHIFT)
#define ADC_DIGCTL_ADCGC_MASK		ADC_DIGCTL_ADCGC(0xf)

/* ADC_FIFOCTL register */
#define ADC_FIFOCTL_ADFRT		BIT(0)
#define ADC_FIFOCTL_ADFFDE		BIT(1)
#define ADC_FIFOCTL_ADFFIE		BIT(2)
#define ADC_FIFOCTL_ADFOS_SHIFT		3
#define ADC_FIFOCTL_ADFOS(x)		((x) << ADC_FIFOCTL_ADFOS_SHIFT)
#define ADC_FIFOCTL_ADFOS_MASK		ADC_FIFOCTL_ADFOS(0x1)
#define	ADC_FIFOCTL_ADFOS_CPU		ADC_FIFOCTL_ADFOS(0)
#define	ADC_FIFOCTL_ADFOS_DMA		ADC_FIFOCTL_ADFOS(1)

/* ADC_STAT register */
#define ADC_STAT_ADFS_SHIFT		0
#define ADC_STAT_ADFS(x)		((x) << ADC_STAT_ADFS_SHIFT)
#define ADC_STAT_ADFS_MASK		ADC_STAT_ADFS(0x1f)
#define ADC_STAT_ADFIP			BIT(5)
#define ADC_STAT_ADFEF			BIT(6)

/* ADC_ANACTL0 register */
#define ADC_ANACTL0_ADREN		BIT(0)
#define ADC_ANACTL0_ADLEN		BIT(1)
#define ADC_ANACTL0_AUX0LEN		BIT(2)
#define ADC_ANACTL0_AUX0REN		BIT(3)
#define ADC_ANACTL0_OP0LEN		BIT(4)
#define ADC_ANACTL0_OP0REN		BIT(5)
#define ADC_ANACTL0_OP0G_SHIFT		6
#define ADC_ANACTL0_OP0G(x)		(((x) & 0x7) << ADC_ANACTL0_OP0G_SHIFT)
#define ADC_ANACTL0_OP0G_MASK		ADC_ANACTL0_OP0G(0x7)
#define ADC_ANACTL0_AUX1LEN		BIT(9)
#define ADC_ANACTL0_AUX1REN		BIT(10)
#define ADC_ANACTL0_OP1LEN		BIT(11)
#define ADC_ANACTL0_OP1REN		BIT(12)
#define ADC_ANACTL0_OP1G_SHIFT		13
#define ADC_ANACTL0_OP1G(x)		(((x) & 0x7) << ADC_ANACTL0_OP1G_SHIFT)
#define ADC_ANACTL0_OP1G_MASK		ADC_ANACTL0_OP1G(0x7)
#define ADC_ANACTL0_AUX0LPDEN		BIT(16)
#define ADC_ANACTL0_AUX0RPDEN		BIT(17)
#define ADC_ANACTL0_AUX1LPDEN		BIT(18)
#define ADC_ANACTL0_AUX1RPDEN		BIT(19)
#define ADC_ANACTL0_ADVREN		BIT(20)
#define ADC_ANACTL0_ADLPFLEN		BIT(21)
#define ADC_ANACTL0_ADLPFREN		BIT(22)
#define ADC_ANACTL0_OP0L2LPFL		BIT(23)
#define ADC_ANACTL0_OP0R2LPFR		BIT(24)
#define ADC_ANACTL0_OP1L2LPFL		BIT(25)
#define ADC_ANACTL0_OP1R2LPFR		BIT(26)

/* ADC_ANACTL1 register */
#define ADC_ANACTL1_MIC0EN		BIT(0)
#define ADC_ANACTL1_MIC0GBEN		BIT(1)
#define ADC_ANACTL1_MIC0G_SHIFT		2
#define ADC_ANACTL1_MIC0G(x)		(((x) & 0xf) << ADC_ANACTL1_MIC0G_SHIFT)
#define ADC_ANACTL1_MIC0G_MASK		ADC_ANACTL1_MIC0G(0xf)
#define ADC_ANACTL1_MOP0EN		BIT(6)
#define ADC_ANACTL1_MIC0GTEN		BIT(7)
#define ADC_ANACTL1_MIC1EN		BIT(8)
#define ADC_ANACTL1_MIC1OPGBEN		BIT(9)
#define ADC_ANACTL1_MIC1G_SHIFT		10
#define ADC_ANACTL1_MIC1G(x)		(((x) & 0xf) << ADC_ANACTL1_MIC1G_SHIFT)
#define ADC_ANACTL1_MIC1G_MASK		ADC_ANACTL1_MIC1G(0xf)
#define ADC_ANACTL1_MOP1EN		BIT(14)
#define ADC_ANACTL1_MIC1GTEN		BIT(15)
#define ADC_ANACTL1_MOP02LPFL		BIT(16)
#define ADC_ANACTL1_MOP12LPFL		BIT(17)
#define ADC_ANACTL1_MOP12LPFR		BIT(18)

/* I2S_RXCTL */
#define I2S_RXCTL_ENABLE		BIT(0)
#define I2S_RXCTL_DATA_FORMAT_MASK		(0x3 << 1)
#define I2S_RXCTL_DATA_FORMAT(x)		(((x) & 0x3) << 1)
#define	I2S_RXCTL_DATA_FORMAT_I2S		I2S_RXCTL_DATA_FORMAT(0)
#define	I2S_RXCTL_DATA_FORMAT_LEFT_JUSTIFIED	I2S_RXCTL_DATA_FORMAT(1)
#define	I2S_RXCTL_DATA_FORMAT_RIGHT_JUSTIFIED	I2S_RXCTL_DATA_FORMAT(2)
#define I2S_RXCTL_BCLK_SET_MASK		(0x1 << 3)
#define I2S_RXCTL_BCLK_SET(x)		(((x) & 0x1) << 3)
#define	I2S_RXCTL_BCLK_SET_64FS		I2S_RXCTL_BCLK_SET(0)
#define	I2S_RXCTL_BCLK_SET_32FS		I2S_RXCTL_BCLK_SET(1)
#define I2S_RXCTL_WIDTH_MASK		(0x3 << 4)
#define I2S_RXCTL_WIDTH(x)		(((x) & 0x3) << 4)
#define	I2S_RXCTL_WIDTH_16BIT		I2S_RXCTL_WIDTH(0)
#define	I2S_RXCTL_WIDTH_20BIT		I2S_RXCTL_WIDTH(1)
#define	I2S_RXCTL_WIDTH_24BIT		I2S_RXCTL_WIDTH(2)
#define I2S_RXCTL_SMCLK_MASK		BIT(6)
#define	I2S_RXCTL_SMCLK_INT		0
#define	I2S_RXCTL_SMCLK_EXT		I2S_RXCTL_SMCLK_MASK
#define I2S_RXCTL_MODE_MASK		BIT(7)
#define	I2S_RXCTL_MODE_MASTER		0
#define	I2S_RXCTL_MODE_SLAVE		I2S_RXCTL_MODE_MASK

/* I2S_FIFOCTL */
#define I2S_RXFIFOCTL_OUTPUT_SEL_MASK		(0x3 << 4)
#define I2S_RXFIFOCTL_OUTPUT_SEL(x)		(((x) & 0x3) << 4)
#define	I2S_RXFIFOCTL_OUTPUT_SEL_CPU		I2S_RXFIFOCTL_OUTPUT_SEL(0)
#define	I2S_RXFIFOCTL_OUTPUT_SEL_DMA		I2S_RXFIFOCTL_OUTPUT_SEL(1)
#define I2S_RXFIFOCTL_INPUT_SEL_MASK		(0x1 << 3)
#define I2S_RXFIFOCTL_INPUT_SEL(x)		(((x) & 0x1) << 3)
#define	I2S_RXFIFOCTL_INPUT_SEL_I2S		I2S_RXFIFOCTL_INPUT_SEL(0)
#define	I2S_RXFIFOCTL_INPUT_SEL_SPDIF		I2S_RXFIFOCTL_INPUT_SEL(1)
#define I2S_RXFIFOCTL_FULL_IRQ_ENABLE		BIT(2)
#define I2S_RXFIFOCTL_FULL_DRQ_ENABLE		BIT(1)
#define I2S_RXFIFOCTL_FIFO_ENABLE			BIT(0)

/* audio adc hardware controller */
struct acts_audio_adc
{
	volatile u32_t digctl;
	volatile u32_t fifoctl;
	volatile u32_t stat;
	volatile u32_t dat;
	volatile u32_t anactl0;
	volatile u32_t anactl1;
	volatile u32_t bias;
};

struct acts_audio_i2s
{
	volatile u32_t txctl;
	volatile u32_t rxctl;
	volatile u32_t rxfifoctl;
	volatile u32_t rxstat;
	volatile u32_t rxdat;
};

struct acts_audio_in_config {
	struct acts_audio_adc *adc;
	u8_t clock_id;
	u8_t reset_id;
	u8_t dma_id;
	u8_t dma_chan;

	struct acts_audio_i2s *i2s;
	u8_t i2s_rx_clock_id;
	u8_t i2s_rx_dma_id;
};

struct acts_audio_in_data {
	pcmbuf_irq_cbk irq_cbk;
	bool hf_ie;
	bool fu_ie;

	struct device *dma_dev;

	u32_t enabled_src;
	atomic_t enabled_cnt;
};

static void audio_in_dma_done_callback(struct device *dev, u32_t callback_data,
				       int reason)
{
	struct acts_audio_in_data *data = (struct acts_audio_in_data *)callback_data;

	if (data->irq_cbk) {
		if (reason > 0 && data->hf_ie)
			data->irq_cbk(PCMBUF_IP_HF);
		else if (data->fu_ie)
			data->irq_cbk(PCMBUF_IP_FU);
	}
}

static void audio_in_config_dma(struct device *dev, uint16_t *pcm_buf, u32_t data_cnt)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_in_data *data = dev->driver_data;
	struct dma_config dma_cfg = {0};
	struct dma_block_config dma_block_cfg = {0};

	dma_cfg.channel_direction = PERIPHERAL_TO_MEMORY;

	if (data->enabled_src & BIT(AIN_SOURCE_I2S0))
		dma_cfg.dma_slot = cfg->i2s_rx_dma_id;
	else
		dma_cfg.dma_slot = cfg->dma_id;

	dma_cfg.dma_callback = audio_in_dma_done_callback;
	dma_cfg.callback_data = data;

	/* 16bit width data */
	dma_cfg.dest_data_size = 2;

	if (data->fu_ie)
		dma_cfg.complete_callback_en = 1;

	if (data->hf_ie)
		dma_cfg.half_complete_callback_en = 1;

	dma_cfg.half_complete_callback_en = 1;
	dma_cfg.error_callback_en = 1;
	dma_cfg.block_count = 1;
	dma_cfg.head_block = &dma_block_cfg;

	dma_block_cfg.block_size = data_cnt;
	dma_block_cfg.dest_address = (u32_t)pcm_buf;
	dma_block_cfg.dest_reload_en = 1;

	dma_config(data->dma_dev, cfg->dma_chan, &dma_cfg);
}

void audio_in_pcmbuf_config(struct device *dev, pcmbuf_setting_t *setting)
{
	struct acts_audio_in_data *data = dev->driver_data;

	data->irq_cbk = setting->irq_cbk;
	data->hf_ie = setting->hf_ie;
	data->fu_ie = setting->fu_ie;
}

u32_t audio_in_pcmbuf_get_length(struct device *dev)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_in_data *data = dev->driver_data;

	/* not implement */
	return dma_get_remain_size(data->dma_dev, cfg->dma_chan);
}

static void audio_in_start_i2s(struct acts_audio_i2s *i2s)
{
	i2s->rxfifoctl |= I2S_RXFIFOCTL_FIFO_ENABLE;
	i2s->rxctl |= I2S_RXCTL_ENABLE;
}

static void audio_in_enable_i2s(const struct acts_audio_in_config *cfg, bool ext_clk)
{
	struct acts_audio_i2s *i2s = cfg->i2s;

	acts_clock_peripheral_enable(cfg->i2s_rx_clock_id);

	acts_clock_peripheral_enable(CLOCK_ID_FM);
	sys_write32(sys_read32(CMU_FMCLK) | 0x03, CMU_FMCLK);

	if (ext_clk) {
		/* choose external I2S clock */
		sys_write32((sys_read32(CMU_I2SCLK) & ~0x300) | 0x200, CMU_I2SCLK);

		i2s->rxctl = I2S_RXCTL_MODE_SLAVE | I2S_RXCTL_SMCLK_EXT |
				I2S_RXCTL_BCLK_SET_64FS;
	} else {
		/* choose internal I2S clock */
		sys_write32(sys_read32(CMU_I2SCLK) & ~0x300, CMU_I2SCLK);

		i2s->rxctl = I2S_RXCTL_MODE_SLAVE | I2S_RXCTL_SMCLK_INT |
				I2S_RXCTL_BCLK_SET_64FS;
	}

	i2s->rxfifoctl = I2S_RXFIFOCTL_OUTPUT_SEL_DMA | I2S_RXFIFOCTL_FULL_DRQ_ENABLE;
}

static void audio_in_disable_i2s(const struct acts_audio_in_config *cfg)
{
	struct acts_audio_i2s *i2s = cfg->i2s;

	i2s->rxfifoctl = 0;
	i2s->rxctl = 0;

	acts_clock_peripheral_disable(cfg->i2s_rx_clock_id);
	acts_clock_peripheral_disable(CLOCK_ID_FM);
}

int audio_in_input_start(struct device *dev, uint16_t *pcm_buf, u32_t data_cnt)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_in_data *data = dev->driver_data;

	if (atomic_get(&data->enabled_cnt) < 1)
		return -EFAULT;

	audio_in_config_dma(dev, pcm_buf, data_cnt);
	dma_start(data->dma_dev, cfg->dma_chan);

	if (data->enabled_src & BIT(AIN_SOURCE_I2S0))
		audio_in_start_i2s(cfg->i2s);

	return 0;
}

void audio_in_input_stop(struct device *dev)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_in_data *data = dev->driver_data;

	dma_stop(data->dma_dev, cfg->dma_chan);
}

#define DAC_ANACTL 0xc005001c
#define DAC_ANACTL_MIC02PAL		BIT(7)
#define DAC_ANACTL_OP0R2DMTR	BIT(22)
#define DAC_ANACTL_MIC12PAR		BIT(31)

static int audio_in_set_mode_internal(struct acts_audio_adc *adc, ain_setting_t *ain_setting)
{
	u32_t anactl0, anactl1, digctl;

	anactl0 = adc->anactl0;
	anactl1 = adc->anactl1;
	digctl = adc->digctl;

	/* AIN_SOURCE_MIC0 */
	if (ain_setting->ain_src == AIN_SOURCE_MIC0) { /* must AIN_INPUT_L */
		/* enable adc left channel */
		if (ain_setting->ain_data_mode & AIN_MODE_RECORDER) {
			anactl0 |= ADC_ANACTL0_ADLEN | ADC_ANACTL0_ADLPFLEN;
			anactl1 |= ADC_ANACTL1_MOP02LPFL;
		} else {
			anactl1 &= ~ADC_ANACTL1_MOP02LPFL;
		}

		/* enable mic0 */
		anactl1 |= ADC_ANACTL1_MIC0EN | ADC_ANACTL1_MOP0EN;

	/* AIN_SOURCE_MIC1 */
	} else if (ain_setting->ain_src == AIN_SOURCE_MIC1) { /* must AIN_INPUT_R */
		/* enable adc right channel */
		if (ain_setting->ain_data_mode & AIN_MODE_RECORDER) {
			anactl0 |= ADC_ANACTL0_ADREN | ADC_ANACTL0_ADLPFREN;
			anactl1 |= ADC_ANACTL1_MOP12LPFR;
		} else {
			anactl1 &= ~ADC_ANACTL1_MOP12LPFR;
		}

		/* enable mic1 */
		anactl1 |= ADC_ANACTL1_MIC1EN | ADC_ANACTL1_MOP1EN;

	/* AIN_SOURCE_AUX0 */
	} else if (ain_setting->ain_src == AIN_SOURCE_AUX0) {
		/* enable aux0 left channel */
		if (ain_setting->ain_input_mode & AIN_INPUT_L) {
			if (ain_setting->ain_data_mode & AIN_MODE_RECORDER) {
				anactl0 |= ADC_ANACTL0_ADLEN | ADC_ANACTL0_ADLPFLEN | ADC_ANACTL0_OP0L2LPFL;
			} else {
				anactl0 &= ~ADC_ANACTL0_OP0L2LPFL;
			}

			anactl0 |= ADC_ANACTL0_AUX0LEN | ADC_ANACTL0_OP0LEN;
		}

		/* enable aux0 right channel */
		if (ain_setting->ain_input_mode & AIN_INPUT_R) {
			if (ain_setting->ain_data_mode & AIN_MODE_RECORDER) {
				anactl0 |= ADC_ANACTL0_ADREN | ADC_ANACTL0_ADLPFREN | ADC_ANACTL0_OP0R2LPFR;
			} else {
				anactl0 &= ~ADC_ANACTL0_OP0R2LPFR;
			}

			anactl0 |= ADC_ANACTL0_AUX0REN | ADC_ANACTL0_OP0REN;
		}

	/* AIN_SOURCE_AUX1 */
	} else if (ain_setting->ain_src == AIN_SOURCE_AUX1) {
		/* enable aux1 left channel */
		if (ain_setting->ain_input_mode & AIN_INPUT_L) {
			if (ain_setting->ain_data_mode & AIN_MODE_RECORDER) {
				anactl0 |= ADC_ANACTL0_ADLEN | ADC_ANACTL0_ADLPFLEN | ADC_ANACTL0_OP1L2LPFL;
			} else {
				anactl0 &= ~ADC_ANACTL0_OP1L2LPFL;
			}

			anactl0 |= ADC_ANACTL0_AUX1LEN | ADC_ANACTL0_OP1LEN;
		}

		/* enable aux1 right channel */
		if (ain_setting->ain_input_mode & AIN_INPUT_R) {
			if (ain_setting->ain_data_mode & AIN_MODE_RECORDER) {
				anactl0 |= ADC_ANACTL0_ADREN | ADC_ANACTL0_ADLPFREN | ADC_ANACTL0_OP1R2LPFR;
			} else {
				anactl0 &= ~ADC_ANACTL0_OP1R2LPFR;
			}

			anactl0 |= ADC_ANACTL0_AUX1REN | ADC_ANACTL0_OP1REN;
		}

	/* AIN_SOURCE_DMIC */
	} else if (ain_setting->ain_src == AIN_SOURCE_DMIC) {  /* must AIN_MODE_RECORDER */
		if (ain_setting->ain_input_mode & AIN_INPUT_L) {
			digctl |= ADC_DIGCTL_DMLEN;
			anactl0 |= ADC_ANACTL0_ADLEN;
		}

		if (ain_setting->ain_input_mode & AIN_INPUT_R) {
			digctl |= ADC_DIGCTL_DMREN;
			anactl0 |= ADC_ANACTL0_ADREN;
		}

	/* AIN_SOURCE_I2S0 */
	} else if (ain_setting->ain_src == AIN_SOURCE_I2S0) {

	}

	adc->anactl0 = anactl0;
	adc->anactl1 = anactl1;
	adc->digctl = digctl;

	return 0;
}

int audio_in_set_mode(struct device *dev, ain_setting_t *ain_setting)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	return audio_in_set_mode_internal(cfg->adc, ain_setting);
}

static int audio_in_set_adc_gain_internal(struct acts_audio_adc *adc, adc_gain_e gain)
{
	adc->digctl = (adc->digctl & ~ADC_DIGCTL_DMICGAIN_MASK) | ADC_DIGCTL_ADCGC(gain);
	return 0;
}

int audio_in_set_adc_gain(struct device *dev, adc_gain_e gain)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	return audio_in_set_adc_gain_internal(cfg->adc, gain);
}

int audio_in_get_adc_gain(struct device *dev)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	return (cfg->adc->digctl & ADC_DIGCTL_DMICGAIN_MASK) >> ADC_DIGCTL_DMICGAIN_SHIFT;
}

static int audio_in_set_gain_internal(struct acts_audio_adc *adc, ain_setting_t *ain_setting)
{
	u32_t anactl0, anactl1, digctl;

	anactl0 = adc->anactl0;
	anactl1 = adc->anactl1;
	digctl = adc->digctl;

	if (ain_setting->ain_src == AIN_SOURCE_MIC0) {
		anactl1 &= ~ADC_ANACTL1_MIC0G_MASK;
		anactl1 |= ADC_ANACTL1_MIC0G(ain_setting->ain_gain.mic_gain.gain);
		if (ain_setting->ain_gain.mic_gain.boost)
			anactl1 |= ADC_ANACTL1_MIC0GBEN;
		else
			anactl1 &= ~ADC_ANACTL1_MIC0GBEN;
	} else if (ain_setting->ain_src == AIN_SOURCE_MIC1) {
		anactl1 &= ~ADC_ANACTL1_MIC1G_MASK;
		anactl1 |= ADC_ANACTL1_MIC1G(ain_setting->ain_gain.mic_gain.gain);
		if (ain_setting->ain_gain.mic_gain.boost)
			anactl1 |= ADC_ANACTL1_MIC1OPGBEN;
		else
			anactl1 &= ~ADC_ANACTL1_MIC1OPGBEN;
	} else if (ain_setting->ain_src == AIN_SOURCE_AUX0) {
		anactl0 &= ~ADC_ANACTL0_OP0G_MASK;
		anactl0 |= ADC_ANACTL0_OP0G(ain_setting->ain_gain.aux_gain);
	} else if (ain_setting->ain_src == AIN_SOURCE_AUX1) {
		anactl0 &= ~ADC_ANACTL0_OP1G_MASK;
		anactl0 |= ADC_ANACTL0_OP1G(ain_setting->ain_gain.aux_gain);
	} else if (ain_setting->ain_src == AIN_SOURCE_DMIC) {
		digctl &= ~ADC_DIGCTL_DMICGAIN_MASK;
		digctl |= ADC_DIGCTL_DMICGAIN(ain_setting->ain_gain.dmic_gain);
	}

	adc->anactl0 = anactl0;
	adc->anactl1 = anactl1;
	adc->digctl = digctl;

	return 0;
}

int audio_in_set_gain(struct device *dev, ain_setting_t *ain_setting)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	return audio_in_set_gain_internal(cfg->adc, ain_setting);
}

static void audio_in_init_clk(const struct acts_audio_in_config *cfg)
{
	acts_clock_audio_init();

	/* deassert audio controller reset */
	//acts_reset_peripheral_deassert(cfg->reset_id);

	/* enable audio controller clock */
	acts_clock_peripheral_enable(cfg->clock_id);
	k_busy_wait(100);
}

static void audio_in_enable_ain(struct acts_audio_adc *adc, ain_setting_t *ain_setting)
{
	audio_in_set_mode_internal(adc, ain_setting);
	audio_in_set_gain_internal(adc, ain_setting);
}

static void audio_in_disable_ain(struct acts_audio_adc *adc, ain_setting_t *ain_setting)
{
	u32_t anactl0, anactl1, digctl;

	anactl0 = adc->anactl0;
	anactl1 = adc->anactl1;
	digctl = adc->digctl;

	/* AIN_SOURCE_MIC0 */
	if (ain_setting->ain_src == AIN_SOURCE_MIC0) { /* must AIN_INPUT_L */
		if (ain_setting->ain_input_mode & AIN_INPUT_L)
			anactl1 &= ~(ADC_ANACTL1_MOP02LPFL | ADC_ANACTL1_MIC0EN | ADC_ANACTL1_MOP0EN);

	/* AIN_SOURCE_MIC1 */
	} else if (ain_setting->ain_src == AIN_SOURCE_MIC1) { /* must AIN_INPUT_R */
		if (ain_setting->ain_input_mode & AIN_INPUT_R)
			anactl1 &= ~(ADC_ANACTL1_MOP12LPFR | ADC_ANACTL1_MIC1EN | ADC_ANACTL1_MOP1EN);

	/* AIN_SOURCE_AUX0 */
	} else if (ain_setting->ain_src == AIN_SOURCE_AUX0) {
		if (ain_setting->ain_input_mode & AIN_INPUT_L)
			anactl0 &= ~(ADC_ANACTL0_OP0L2LPFL | ADC_ANACTL0_AUX0LEN | ADC_ANACTL0_OP0LEN);
		if (ain_setting->ain_input_mode & AIN_INPUT_R)
			anactl0 &= ~(ADC_ANACTL0_OP0R2LPFR | ADC_ANACTL0_AUX0REN | ADC_ANACTL0_OP0REN);

	/* AIN_SOURCE_AUX1 */
	} else if (ain_setting->ain_src == AIN_SOURCE_AUX1) {
		if (ain_setting->ain_input_mode & AIN_INPUT_L)
			anactl0 &= ~(ADC_ANACTL0_OP1L2LPFL | ADC_ANACTL0_AUX1LEN | ADC_ANACTL0_OP1LEN);
		if (ain_setting->ain_input_mode & AIN_INPUT_R)
			anactl0 &= ~(ADC_ANACTL0_OP1R2LPFR | ADC_ANACTL0_AUX1REN | ADC_ANACTL0_OP1REN);

	/* AIN_SOURCE_DMIC */
	} else if (ain_setting->ain_src == AIN_SOURCE_DMIC) {
		if (ain_setting->ain_input_mode & AIN_INPUT_L)
			digctl &= ~ADC_DIGCTL_DMLEN;
		if (ain_setting->ain_input_mode & AIN_INPUT_R)
			digctl &= ~ADC_DIGCTL_DMREN;

	/* AIN_SOURCE_I2S0 */
	} else if (ain_setting->ain_src == AIN_SOURCE_I2S0) {

 	}

	adc->anactl0 = anactl0;
	adc->anactl1 = anactl1;
	adc->digctl = digctl;
}

int audio_in_enable(struct device *dev, ain_setting_t *ain_setting,
		     adc_setting_t *adc_setting)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_in_data *data = dev->driver_data;
	struct acts_audio_adc *adc = cfg->adc;
	bool ext_clk = false;

	ACT_LOG_ID_DBG(ALF_STR_audio_in_enable__ENABLE_AUDIO_ADC, 0);

	if (atomic_inc(&data->enabled_cnt) == 0) {
		audio_in_init_clk(cfg);

		/*
		 * adc hpf enable, adc gain set 0db
		 */
		adc->digctl = ADC_DIGCTL_HPFREN | ADC_DIGCTL_HPFLEN | ADC_DIGCTL_ADCGC(ADC_GAIN_0DB);

		/* clear fifo reset, fifo output to dma, enable fifo full drq */
		adc->fifoctl = ADC_FIFOCTL_ADFRT | ADC_FIFOCTL_ADFFDE | ADC_FIFOCTL_ADFOS_DMA;
		adc->bias = 0xa800556f;

		/* reset anaclt* set in audio_in_init */
		adc->anactl0 = ADC_ANACTL0_ADVREN;
		adc->anactl1 = 0;
	}

	/* if AIN_INPUT_I2S_EXT_CLK set, must be AIN_SOURCE_I2S0 */
	ext_clk = (ain_setting->ain_input_mode & AIN_INPUT_I2S_EXT_CLK);

	if (!ext_clk)
		acts_clock_audioin_set_rate(adc_setting->sample_rate);

	if (ain_setting->ain_src == AIN_SOURCE_I2S0) {
		audio_in_enable_i2s(cfg, ext_clk);
	} else {
		audio_in_set_adc_gain_internal(adc, adc_setting->gain);
		audio_in_enable_ain(adc, ain_setting);
	}

	data->enabled_src |= BIT(ain_setting->ain_src);

	return 0;
}

int audio_in_enable_channels(struct device *dev,
		ain_channel_setting_t *channel_setting, adc_setting_t *adc_setting)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_in_data *data = dev->driver_data;
	struct acts_audio_adc *adc = cfg->adc;
	bool ext_clk = false;

	ACT_LOG_ID_DBG(ALF_STR_audio_in_enable_channels__ENABLE_AUDIO_ADC, 0);

	if (atomic_inc(&data->enabled_cnt) == 0) {
		audio_in_init_clk(cfg);

		/*
		 * adc hpf enable, adc gain set 0db
		 */
		adc->digctl = ADC_DIGCTL_HPFREN | ADC_DIGCTL_HPFLEN | ADC_DIGCTL_ADCGC(ADC_GAIN_0DB);

		/* clear fifo reset, fifo output to dma, enable fifo full drq */
		adc->fifoctl = ADC_FIFOCTL_ADFRT | ADC_FIFOCTL_ADFFDE | ADC_FIFOCTL_ADFOS_DMA;
		adc->bias = 0xa800556f;

		/* reset anaclt* set in audio_in_init */
		adc->anactl0 = ADC_ANACTL0_ADVREN;
		adc->anactl1 = 0;
	}

	/* if AIN_INPUT_I2S_EXT_CLK set, must be AIN_SOURCE_I2S0 */
	ext_clk = (channel_setting->setting_l.ain_input_mode & AIN_INPUT_I2S_EXT_CLK) ||
			(channel_setting->setting_r.ain_input_mode & AIN_INPUT_I2S_EXT_CLK);
	if (!ext_clk)
		acts_clock_audioin_set_rate(adc_setting->sample_rate);

	if ((channel_setting->setting_l.ain_src == AIN_SOURCE_I2S0) ||
		(channel_setting->setting_r.ain_src == AIN_SOURCE_I2S0)) {
		audio_in_enable_i2s(cfg, ext_clk);
		data->enabled_src |= BIT(AIN_SOURCE_I2S0);
	} else {
		audio_in_set_adc_gain_internal(adc, adc_setting->gain);

		if (channel_setting->setting_l.ain_src != AIN_SOURCE_NONE) {
			channel_setting->setting_l.ain_input_mode = AIN_INPUT_L;
			audio_in_enable_ain(adc, &channel_setting->setting_l);
		}

		if (channel_setting->setting_r.ain_src != AIN_SOURCE_NONE) {
			channel_setting->setting_r.ain_input_mode = AIN_INPUT_R;
			audio_in_enable_ain(adc, &channel_setting->setting_r);
		}

		data->enabled_src |= BIT(channel_setting->setting_l.ain_src);
		data->enabled_src |= BIT(channel_setting->setting_r.ain_src);
	}

	return 0;
}

void audio_in_disable(struct device *dev, ain_setting_t *ain_setting)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_in_data *data = dev->driver_data;
	struct acts_audio_adc *adc = cfg->adc;

	if (atomic_get(&data->enabled_cnt) <= 0)
		return;

	if (ain_setting->ain_src == AIN_SOURCE_I2S0)
		audio_in_disable_i2s(cfg);
	else
		audio_in_disable_ain(adc, ain_setting);

	data->enabled_src &= ~BIT(ain_setting->ain_src);

	if (atomic_dec(&data->enabled_cnt) == 1) {
		dma_stop(data->dma_dev, cfg->dma_chan);

		/* ADC_ANACTL0_ADLEN and ADC_ANACTL0_ADREN must be disabled */
		adc->anactl0 = 0;
		adc->anactl1 = 0;
		/* ADC_FIFOCTL_ADFRT must be asserted */
		adc->fifoctl = 0;

		/* disable audio ADC clock */
		acts_clock_peripheral_disable(cfg->clock_id);
	}
}

void audio_in_disable_channels(struct device *dev, ain_channel_setting_t *channel_setting)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_in_data *data = dev->driver_data;
	struct acts_audio_adc *adc = cfg->adc;

	if (atomic_get(&data->enabled_cnt) <= 0)
		return;

	if ((channel_setting->setting_l.ain_src == AIN_SOURCE_I2S0) ||
		(channel_setting->setting_r.ain_src == AIN_SOURCE_I2S0)) {
		audio_in_disable_i2s(cfg);

		data->enabled_src &= ~BIT(AIN_SOURCE_I2S0);
	} else {
		if (channel_setting->setting_l.ain_src != AIN_SOURCE_NONE) {
			channel_setting->setting_l.ain_input_mode = AIN_INPUT_L;
			audio_in_disable_ain(adc, &channel_setting->setting_l);
		}

		if (channel_setting->setting_r.ain_src != AIN_SOURCE_NONE) {
			channel_setting->setting_r.ain_input_mode = AIN_INPUT_R;
			audio_in_disable_ain(adc, &channel_setting->setting_r);
		}

		data->enabled_src &= ~BIT(channel_setting->setting_l.ain_src);
		data->enabled_src &= ~BIT(channel_setting->setting_r.ain_src);
	}

	if (atomic_dec(&data->enabled_cnt) == 1) {
		dma_stop(data->dma_dev, cfg->dma_chan);

		/* ADC_ANACTL0_ADLEN and ADC_ANACTL0_ADREN must be disabled */
		adc->anactl0 = 0;
		adc->anactl1 = 0;
		/* ADC_FIFOCTL_ADFRT must be asserted */
		adc->fifoctl = 0;

		/* disable audio ADC clock */
		acts_clock_peripheral_disable(cfg->clock_id);
	}
}

int audio_in_init(struct device *dev)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_in_data *data = dev->driver_data;
	struct acts_audio_adc *adc = cfg->adc;

	ACT_LOG_ID_DBG(ALF_STR_audio_in_init__INIT_AUDIO_INPUT, 0);

	data->dma_dev = device_get_binding(CONFIG_DMA_0_NAME);
	if (!data->dma_dev)
		return -ENODEV;

	acts_reset_peripheral_deassert(cfg->reset_id);

	audio_in_init_clk(cfg);

	/**
	  * enable mic first time
	  * avoid first recording waveform is abnormal
	  */

	adc->anactl0 |= ADC_ANACTL0_ADLEN | ADC_ANACTL0_ADLPFLEN
					| ADC_ANACTL0_ADREN | ADC_ANACTL0_ADLPFREN;
	adc->anactl1 |= ADC_ANACTL1_MIC0EN | ADC_ANACTL1_MOP0EN | ADC_ANACTL1_MOP02LPFL
					| ADC_ANACTL1_MIC1EN | ADC_ANACTL1_MOP1EN | ADC_ANACTL1_MOP12LPFR;

	/* disable audio ADC */
	adc->anactl0 &= ~ADC_ANACTL0_ADLEN;
	adc->anactl0 &= ~ADC_ANACTL0_ADREN;

	/* disable audio ADC clock */
	acts_clock_peripheral_disable(cfg->clock_id);

	/* dummy driver api to make device_get_binding() happy */
	dev->driver_api = (const void *)0x12345678;

	return 0;
}

static struct acts_audio_in_data audio_in_acts_data;

static const struct acts_audio_in_config audio_in_acts_config = {
	.adc = (struct acts_audio_adc *) AUDIO_ADC_REG_BASE,
	.clock_id = CLOCK_ID_AUDIO_ADC,
	.reset_id = RESET_ID_AUDIO,
	.dma_chan = CONFIG_AUDIO_IN_ACTS_DMA_CHAN,
	.dma_id = DMA_ID_AUDIO_ADC,

	.i2s = (struct acts_audio_i2s *) AUDIO_I2S_REG_BASE,
	.i2s_rx_clock_id = CLOCK_ID_I2S_RX,
	.i2s_rx_dma_id = DMA_ID_I2S_RX,
};

DEVICE_INIT(audio_in, CONFIG_AUDIO_IN_ACTS_DEV_NAME, audio_in_init,
	&audio_in_acts_data, &audio_in_acts_config,
	POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
