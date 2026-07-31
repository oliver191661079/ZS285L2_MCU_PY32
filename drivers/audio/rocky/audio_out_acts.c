/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Audio output driver for Actions SoC
 */

#include <kernel.h>
#include <init.h>
#include <device.h>
#include <irq.h>
#include <dma.h>
#include <soc.h>
#include <string.h>
#include <audio.h>
#include <gpio.h>
#include <board.h>
#include <atomic.h>

#define SYS_LOG_DOMAIN "audio_out"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_AUDIO_OUT_ACTS

/* register defines */

/* DAC_DIGCTL register */
/* DAC digital enable */
#define DAC_DIGCTL_DDEN			BIT(0)
/* DAC Dith enable */
#define DAC_DIGCTL_ENDITH		BIT(1)
/* DAC over samping rate */
#define DAC_DIGCTL_OSRDA_SHIFT		2
#define DAC_DIGCTL_OSRDA(x)		((x) << DAC_DIGCTL_OSRDA_SHIFT)
#define DAC_DIGCTL_OSRDA_MASK		DAC_DIGCTL_OSRDA(0x3)
#define		DAC_DIGCTL_OSRDA_32X		DAC_DIGCTL_OSRDA(0)
#define		DAC_DIGCTL_OSRDA_64X		DAC_DIGCTL_OSRDA(1)
#define		DAC_DIGCTL_OSRDA_128X		DAC_DIGCTL_OSRDA(2)
#define		DAC_DIGCTL_OSRDA_256X		DAC_DIGCTL_OSRDA(3)
/* DAC interpolation filter rate */
#define DAC_DIGCTL_INTFRS_SHIFT		4
#define DAC_DIGCTL_INTFRS(x)		((x) << DAC_DIGCTL_INTFRS_SHIFT)
#define DAC_DIGCTL_INTFRS_MASK		DAC_DIGCTL_INTFRS(0x3)
#define		DAC_DIGCTL_INTFRS_1X		DAC_DIGCTL_INTFRS(0)
#define		DAC_DIGCTL_INTFRS_2X		DAC_DIGCTL_INTFRS(1)
#define		DAC_DIGCTL_INTFRS_4X		DAC_DIGCTL_INTFRS(2)
#define		DAC_DIGCTL_INTFRS_8X		DAC_DIGCTL_INTFRS(3)
/* reset SDM after has detect noise */
#define DAC_DIGCTL_SDMREEN		BIT(6)
/* select ADC channel to DMA L&R digital loop back */
#define DAC_DIGCTL_LBCHNS_SHIFT		7
#define DAC_DIGCTL_LBCHNS(x)		((x) << DAC_DIGCTL_LBCHNS_SHIFT)
#define DAC_DIGCTL_LBCHNS_MASK		DAC_DIGCTL_LBCHNS(0x1)
#define		DAC_DIGCTL_LBCHNS_R		DAC_DIGCTL_LBCHNS(0)
#define		DAC_DIGCTL_LBCHNS_L		DAC_DIGCTL_LBCHNS(1)
/* enable ADC channel to DMA L&R digital loop back */
#define DAC_DIGCTL_AD2DALPEN		BIT(8)
/* DAC channel number select */
#define DAC_DIGCTL_DACHNUM_SHIFT	9
#define DAC_DIGCTL_DACHNUM(x)		((x) << DAC_DIGCTL_DACHNUM_SHIFT)
#define DAC_DIGCTL_DACHNUM_MASK		DAC_DIGCTL_DACHNUM(0x1)
#define	DAC_DIGCTL_DACHNUM_STEREO	DAC_DIGCTL_DACHNUM(0)
#define	DAC_DIGCTL_DACHNUM_MONO		DAC_DIGCTL_DACHNUM(1)
/* DAC left channel mix to right channel */
#define DAC_DIGCTL_DALRMIX		BIT(10)
/* DAC fifo reset, 0: reset, 1: enable */
#define DAC_DIGCTL_DAFRT		BIT(12)
/* DAC FIFO empty DRQ enable */
#define DAC_DIGCTL_DAFEDE		BIT(13)
/* DAC FIFO empty IRQ enable */
#define DAC_DIGCTL_DAFEIE		BIT(14)
/* DAC FIFO input select */
#define DAC_DIGCTL_DAFIS_SHIFT		15
#define DAC_DIGCTL_DAFIS(x)		((x) << DAC_DIGCTL_DAFIS_SHIFT)
#define DAC_DIGCTL_DAFIS_MASK		DAC_DIGCTL_DAFIS(0x1)
#define		DAC_DIGCTL_DAFIS_DMA		DAC_DIGCTL_DAFIS(0)
#define		DAC_DIGCTL_DAFIS_CPU		DAC_DIGCTL_DAFIS(1)
/* DAC digital debug enable */
#define DAC_DIGCTL_DDDEN		BIT(16)
/* DAC analog debug enable */
#define DAC_DIGCTL_DADEN		BIT(17)
/* DAC debug channel select */
#define DAC_DIGCTL_DADCS_SHIFT		18
#define DAC_DIGCTL_DADCS(x)		((x) << DAC_DIGCTL_DADCS_SHIFT)
#define DAC_DIGCTL_DADCS_MASK		DAC_DIGCTL_DADCS(0x1)
#define		DAC_DIGCTL_DADCS_L		DAC_DIGCTL_DADCS(0)
#define		DAC_DIGCTL_DADCS_R		DAC_DIGCTL_DADCS(1)
/* DAC 128fs/256fs select */
#define DAC_DIGCTL_AUDIO_FS_SHIFT	19
#define DAC_DIGCTL_AUDIO_FS(x)		((x) << DAC_DIGCTL_AUDIO_FS_SHIFT)
#define DAC_DIGCTL_AUDIO_FS_MASK	DAC_DIGCTL_AUDIO_FS(0x1)
#define		DAC_DIGCTL_AUDIO_FS_128FS	DAC_DIGCTL_AUDIO_FS(0)
#define		DAC_DIGCTL_AUDIO_FS_256FS	DAC_DIGCTL_AUDIO_FS(1)
/* SDM noise detection threhold */
#define DAC_DIGCTL_SDMNDTH_SHIFT	20
#define DAC_DIGCTL_SDMNDTH(x)		((x) << DAC_DIGCTL_SDMNDTH_SHIFT)
#define DAC_DIGCTL_SDMNDTH_MASK		DAC_DIGCTL_SDMNDTH(0x1)
#define		DAC_DIGCTL_SDMNDTH_14BIT	DAC_DIGCTL_SDMNDTH(0)
#define		DAC_DIGCTL_SDMNDTH_13BIT	DAC_DIGCTL_SDMNDTH(1)
#define		DAC_DIGCTL_SDMNDTH_12BIT	DAC_DIGCTL_SDMNDTH(2)
#define		DAC_DIGCTL_SDMNDTH_11BIT	DAC_DIGCTL_SDMNDTH(3)


/* DAC_STAT register */
#define DAC_STAT_DAFEIP			BIT(5)

/* PCM_BUF_CTL register */
#define PCM_BUF_CTL_EPIE		BIT(7)
#define PCM_BUF_CTL_FUIE		BIT(6)
#define PCM_BUF_CTL_HEIE		BIT(5)
#define PCM_BUF_CTL_HFIE		BIT(4)
#define PCM_BUF_CTL_IE_MASK		(0xf << 4)

/* PCM_BUF_STAT register */
#define PCM_BUF_STAT_EPIP		BIT(19)
#define PCM_BUF_STAT_FUIP		BIT(18)
#define PCM_BUF_STAT_HEIP		BIT(17)
#define PCM_BUF_STAT_HFIP		BIT(16)
#define PCM_BUF_STAT_IP_MASK		(0xf << 16)
#define PCM_BUF_STAT_PCMBS_SHIFT	18
#define PCM_BUF_STAT_PCMBS(x)		((x) << PCM_BUF_STAT_PCMBS_SHIFT)
#define PCM_BUF_STAT_PCMBS_MASK		PCM_BUF_STAT_PCMBS(0x1fff)

/* DAC_ANACTL register */
#define DAC_ANACTL_DAENL		BIT(0)
#define DAC_ANACTL_DAENR		BIT(1)
#define DAC_ANACTL_PAENL		BIT(2)
#define DAC_ANACTL_PAENR		BIT(3)
#define DAC_ANACTL_PAOSENL		BIT(4)
#define DAC_ANACTL_PAOSENR		BIT(5)
#define DAC_ANACTL_BIASEN		BIT(6)
#define DAC_ANACTL_MIC02PAL		BIT(7)
#define DAC_ANACTL_DPBML		BIT(8)
#define DAC_ANACTL_DPBMR		BIT(9)
#define DAC_ANACTL_OP0L2DMTL		BIT(10)
#define DAC_ANACTL_OP1L2DMTL		BIT(11)
#define DAC_ANACTL_ZERODT		BIT(12)
#define DAC_ANACTL_ATPLP2		BIT(13)
#define DAC_ANACTL_PAVDC		BIT(14)
#define DAC_ANACTL_ATP2CEL		BIT(15)
#define DAC_ANACTL_ATP2CER		BIT(16)
#define DAC_ANACTL_P2IB			BIT(17)
#define DAC_ANACTL_PAIQ_SHIFT		18
#define DAC_ANACTL_PAIQ(x)		((x) << DAC_ANACTL_PAIQ_SHIFT)
#define DAC_ANACTL_PAIQ_MASK		DAC_ANACTL_PAIQ(0x3)
#define DAC_ANACTL_OP0L2PAR		BIT(20)
#define DAC_ANACTL_OP0R2PAL		BIT(21)
#define DAC_ANACTL_OP0R2DMTR		BIT(22)
#define DAC_ANACTL_OP1R2DMTR		BIT(23)
#define DAC_ANACTL_OP0R2PAL_SHIFT	24
#define DAC_ANACTL_OPVROOSIQ(x)		((x) << DAC_ANACTL_OPVROOSIQ_SHIFT)
#define DAC_ANACTL_OPVROOSIQ_MASK		DAC_ANACTL_OPVROOSIQ(0x7)
#define DAC_ANACTL_OPVROEN		BIT(27)
#define DAC_ANACTL_DDATPR		BIT(28)
#define DAC_ANACTL_DDOVV		BIT(29)
#define DAC_ANACTL_OVLS			BIT(30)
#define DAC_ANACTL_MIC12PAR		BIT(31)

/* DAV_VOL register */
#define DAC_VOL_VOLZCEN		(1 << 20)
#define DAC_VOL_VOLZCTOEN	(1 << 21)
#define DAC_VOL_VOL_MASK	(0xFFFFF << 0)

#define DAC_VOL_REG_0DB		0x4000
#define DAC_VOL_MIN_VOL		-600	/* -60db */

/* I2S_TXCTL */
#define I2S_TXCTL_MODE_MASK		BIT(7)
#define	I2S_TXCTL_MODE_MASTER		0
#define	I2S_TXCTL_MODE_SLAVE		I2S_TXCTL_MODE_MASK
#define I2S_TXCTL_SMCLK_MASK		BIT(6)
#define	I2S_TXCTL_SMCLK_INT		0
#define	I2S_TXCTL_SMCLK_EXT		I2S_TXCTL_SMCLK_MASK
#define I2S_TXCTL_WIDTH_MASK		(0x3 << 4)
#define I2S_TXCTL_WIDTH(x)		(((x) & 0x3) << 4)
#define	I2S_TXCTL_WIDTH_16BIT		I2S_TXCTL_WIDTH(0)
#define	I2S_TXCTL_WIDTH_20BIT		I2S_TXCTL_WIDTH(1)
#define	I2S_TXCTL_WIDTH_24BIT		I2S_TXCTL_WIDTH(2)
#define I2S_TXCTL_BCLK_SET_MASK		(0x1 << 3)
#define I2S_TXCTL_BCLK_SET(x)		(((x) & 0x1) << 3)
#define	I2S_TXCTL_BCLK_SET_64FS		I2S_TXCTL_BCLK_SET(0)
#define	I2S_TXCTL_BCLK_SET_32FS		I2S_TXCTL_BCLK_SET(1)
#define I2S_TXCTL_DATA_FORMAT_MASK		(0x3 << 1)
#define I2S_TXCTL_DATA_FORMAT(x)		(((x) & 0x3) << 1)
#define	I2S_TXCTL_DATA_FORMAT_I2S		I2S_TXCTL_DATA_FORMAT(0)
#define	I2S_TXCTL_DATA_FORMAT_LEFT_JUSTIFIED	I2S_TXCTL_DATA_FORMAT(1)
#define	I2S_TXCTL_DATA_FORMAT_RIGHT_JUSTIFIED	I2S_TXCTL_DATA_FORMAT(2)
#define I2S_TXCTL_ENABLE(x)		BIT(0)


/* audio adc hardware controller */
struct acts_audio_dac
{
	volatile u32_t digctl;
	volatile u32_t stat;
	volatile u32_t dat;
	volatile u32_t pcm_buf_ctl;
	volatile u32_t pcm_buf_stat;
	volatile u32_t pcm_buf_thres_he;
	volatile u32_t pcm_buf_thres_hf;
	volatile u32_t anactl;
	volatile u32_t bias;
	volatile u32_t vol;
};

struct acts_audio_i2s
{
	volatile u32_t txctl;
};

struct acts_audio_out_config {
	struct acts_audio_dac *dac;
	u8_t clock_id;
	u8_t reset_id;
	u8_t dma_id;
	u8_t dma_chan;

	void (*irq_config)(void);

	struct acts_audio_i2s *i2s;
	u8_t i2s_clock_id;
};

struct acts_audio_out_data {
	pcmbuf_irq_cbk irq_cbk;
	struct device *dma_dev;
	struct device *gpio_dev;
	uint32_t enabled_dst_type;
	atomic_t enabled_cnt;
};

static void audio_out_pcmbuf_isr(void* arg)
{
	struct device *dev = arg;
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_out_data *data = dev->driver_data;
	struct acts_audio_dac *dac = cfg->dac;
	u32_t stat, pending = 0;

	stat = dac->pcm_buf_stat;

	if (stat & PCM_BUF_STAT_HEIP)
		pending |= PCMBUF_IP_HE;

	if (stat & PCM_BUF_STAT_EPIP)
		pending |= PCMBUF_IP_EP;

	if (stat & PCM_BUF_STAT_HFIP)
		pending |= PCMBUF_IP_HF;

	if (stat & PCM_BUF_STAT_FUIP)
		pending |= PCMBUF_IP_FU;

	dac->pcm_buf_stat = stat;

	if (pending && data->irq_cbk)
		data->irq_cbk(pending);
}

void audio_out_pcmbuf_config(struct device *dev, pcmbuf_setting_t *setting)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_out_data *data = dev->driver_data;
	struct acts_audio_dac *dac = cfg->dac;
	u32_t val;

	/* clear pcm buf irq pending */
	dac->pcm_buf_stat |= PCM_BUF_STAT_IP_MASK;

	/* set half empty threshold */
	dac->pcm_buf_thres_he = setting->half_empty_thres;

	/* set half full threshold */
	dac->pcm_buf_thres_hf = setting->half_full_thres;

	if (setting->he_ie || setting->ep_ie || setting->hf_ie ||
	    setting->fu_ie) {
		val = 0;
		if (setting->he_ie)
			val |= PCM_BUF_CTL_HEIE;
		if (setting->ep_ie)
			val |= PCM_BUF_CTL_EPIE;
		if (setting->hf_ie)
			val |= PCM_BUF_CTL_HFIE;
		if (setting->fu_ie)
			val |= PCM_BUF_CTL_FUIE;

		dac->pcm_buf_ctl = val;
		data->irq_cbk = setting->irq_cbk;
	}
}

u32_t audio_out_pcmbuf_get_length(struct device *dev)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac = cfg->dac;
	u32_t empty_len = 0xffff, tmp_empty_len;
	u32_t flag;
	u8_t i, debounce = 0;

	flag = irq_lock();
	for (i = 0; i < 20; i++) {
		tmp_empty_len = (dac->pcm_buf_stat & PCM_BUF_STAT_PCMBS_MASK) >>
				PCM_BUF_STAT_PCMBS_SHIFT;
		if (tmp_empty_len == empty_len) {
			debounce++;
			if (debounce >= 5)
				break;
		} else {
			empty_len = tmp_empty_len;
			debounce = 0;
		}
	}
	irq_unlock(flag);

	return (0x1000 - empty_len);
}

void audio_out_pcmbuf_reset(struct device *dev)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_out_data *data = dev->driver_data;
	struct acts_audio_dac *dac = cfg->dac;

	dac->pcm_buf_ctl = 0;
	data->irq_cbk = NULL;
}

static void wait_dacfifo_empty(struct acts_audio_dac *dac)
{
	while (!(dac->stat & DAC_STAT_DAFEIP))
		;

	/* clear status */
	dac->stat |= DAC_STAT_DAFEIP;
}

static void audio_out_config_dma(struct device *dev, const uint16_t *pcm_buf, u32_t data_cnt)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_out_data *data = dev->driver_data;
	struct dma_config dma_cfg = {0};
	struct dma_block_config dma_block_cfg = {0};

	dma_cfg.channel_direction = MEMORY_TO_PERIPHERAL;
	dma_cfg.dma_slot = cfg->dma_id;

	/* 16bit width data */
	dma_cfg.source_data_size = 2;

	/* we use pcm irq to track the transfer, so no need dma irq callback */
	dma_cfg.dma_callback = NULL;

	dma_cfg.block_count = 1;
	dma_cfg.head_block = &dma_block_cfg;

	dma_block_cfg.block_size = data_cnt;
	dma_block_cfg.source_address = (u32_t)pcm_buf;

	dma_config(data->dma_dev, cfg->dma_chan, &dma_cfg);
}

void audio_out_output(struct device *dev, const uint16_t *pcm_buf, u32_t data_cnt)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_out_data *data = dev->driver_data;
	struct acts_audio_dac *dac = cfg->dac;

	audio_out_config_dma(dev, pcm_buf, data_cnt);

	dma_start(data->dma_dev, cfg->dma_chan);

	if (data->enabled_dst_type & BIT(DAC_DST_I2S_TX))
		cfg->i2s->txctl |= 0x1;

	dma_wait_finished(data->dma_dev, cfg->dma_chan);

	wait_dacfifo_empty(dac);
}

#define EE			364841611           /* Q27 */
#define INV_EE			49375942            /* Q27 */

#define MUL32_32(a,b)		(long long)((long long)(a)*(long long)(b))
#define MUL32_32_RS15(a,b)	(int)(((long long)(a)*(long long)(b))>>15)
#define MUL32_32_RS(a,b,c)	(int)(((long long)(a)*(long long)(b))>>(c))

const int exp_taylor_coeff[6] =
	{ 134217728, 67108864, 22369621, 5592405, 1118481, 186413 };

/* db: 0.1db */
int libc_math_exp_fixed(int db)
{
	int i, xin, ret;
	int temp = 32768;
	long long sum = 4398046511104L;
	int count = 0;

	xin = (db * 75451) / 200;

	while (xin < -32768) {
		xin += 32768;
		count++;
	}

	while (xin > 32768) {
		xin -= 32768;
		count++;
	}

	for (i = 0; i < 6; i++) {
		temp = MUL32_32_RS15(temp, xin);
		sum += MUL32_32(temp, exp_taylor_coeff[i]);
	}

	ret = (int) (sum >> 27);

	if (count > 0) {
		if (db < 0) {
		    for (i = 0; i < count; i++)
			ret = MUL32_32_RS(ret, INV_EE, 27);
		} else {
		    for (i = 0; i < count; i++)
			ret = MUL32_32_RS(ret, EE, 27);
		}
	}

	return ret;
}

/*
 * 0db(vol_p1_db = 0) ~ -60db(vol_p1_db = -600)
 * ol_p1_db < -600 for mute
 */
int audio_out_set_volume(struct device *dev, int32_t vol_p1_db)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac = cfg->dac;
	u32_t vol;

	if (vol_p1_db > 30)
		return -EINVAL;

	/* enable  DAC clock divisor */
	dac->vol |= DAC_VOL_VOLZCEN | DAC_VOL_VOLZCTOEN;

	if (vol_p1_db <= DAC_VOL_MIN_VOL) {
		dac->vol &= ~DAC_VOL_VOL_MASK;
		return 0;
	}

	vol = libc_math_exp_fixed(vol_p1_db);

	dac->vol &= ~DAC_VOL_VOL_MASK;
	dac->vol |= (DAC_VOL_REG_0DB * vol / 32768) & DAC_VOL_VOL_MASK;

	return 0;
}

static void audio_out_init_clk(const struct acts_audio_out_config *cfg,
			       dac_setting_t *setting)
{
	acts_clock_audio_init();
	if (setting->dst_type == DAC_DST_I2S_TX)
		acts_clock_peripheral_enable(cfg->i2s_clock_id);

	/* switch PCMRAM clock to DAC clock */
	sys_write32(sys_read32(CMU_MEMCLKSEL) | (0x1 << 3), CMU_MEMCLKSEL);

	/* deassert audio controller reset */
	//acts_reset_peripheral_deassert(cfg->reset_id);

	/* enable audio controller clock */
	acts_clock_peripheral_enable(cfg->clock_id);
	k_busy_wait(100);
}

int audio_out_enable_pa_source(struct device *dev, dac_setting_t *setting)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac = cfg->dac;
	u32_t anactl = dac->anactl;

	if (setting->pal_src != AOUT_SOURCE_NONE) {
		/* rest source config */
		anactl &= ~(DAC_ANACTL_DAENL | DAC_ANACTL_DPBML |
					DAC_ANACTL_MIC02PAL | DAC_ANACTL_OP0L2DMTL |
					DAC_ANACTL_OP1L2DMTL | DAC_ANACTL_OP0R2PAL);
		//anactl |= DAC_ANACTL_PAENL | DAC_ANACTL_PAOSENL;

		switch (setting->pal_src) {
		case AOUT_SOURCE_PCM_SINGLE:
		case AOUT_SOURCE_PCM_DOUBLE:
			anactl |= DAC_ANACTL_DAENL | DAC_ANACTL_DPBML;
			break;
		case AOUT_SOURCE_MIC0:
			anactl |= DAC_ANACTL_MIC02PAL;
			break;
		case AOUT_SOURCE_AUX0L:
			anactl |= DAC_ANACTL_OP0L2DMTL;
			break;
		case AOUT_SOURCE_AUX0R:
			anactl |= DAC_ANACTL_OP0R2PAL;
			break;
		case AOUT_SOURCE_AUX1:
			anactl |= DAC_ANACTL_OP0L2DMTL;
			break;
		default:
			return -EINVAL;
		}
	}

	if (setting->par_src != AOUT_SOURCE_NONE) {
		/* rest source config */
		anactl &= ~(DAC_ANACTL_DAENR | DAC_ANACTL_DPBMR |
					DAC_ANACTL_MIC12PAR | DAC_ANACTL_OP0R2DMTR |
					DAC_ANACTL_OP1R2DMTR | DAC_ANACTL_OP0L2PAR);
		//anactl |= DAC_ANACTL_PAENR | DAC_ANACTL_PAOSENR;

		switch (setting->par_src) {
		case AOUT_SOURCE_PCM_SINGLE:
		case AOUT_SOURCE_PCM_DOUBLE:
			anactl |= DAC_ANACTL_DAENR | DAC_ANACTL_DPBMR;
			break;
		case AOUT_SOURCE_MIC1:
			anactl |= DAC_ANACTL_MIC12PAR;
			break;
		case AOUT_SOURCE_AUX0L:
			anactl |= DAC_ANACTL_OP0L2PAR;
			break;
		case AOUT_SOURCE_AUX0R:
			anactl |= DAC_ANACTL_OP0R2DMTR;
			break;
		case AOUT_SOURCE_AUX1:
			anactl |= DAC_ANACTL_OP1R2DMTR;
			break;
		default:
			return -EINVAL;
		}
	}

	dac->anactl = anactl;

	return 0;
}

void audio_out_disable_pa_source(struct device *dev, dac_setting_t *setting)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac = cfg->dac;
	u32_t anactl = dac->anactl;

	if (setting->pal_src != AOUT_SOURCE_NONE) {
		switch (setting->pal_src) {
		case AOUT_SOURCE_PCM_SINGLE:
		case AOUT_SOURCE_PCM_DOUBLE:
			anactl &= ~(DAC_ANACTL_DAENL | DAC_ANACTL_DPBML);
			break;
		case AOUT_SOURCE_MIC0:
			anactl &= ~DAC_ANACTL_MIC02PAL;
			break;
		case AOUT_SOURCE_AUX0L:
			anactl &= ~DAC_ANACTL_OP0L2DMTL;
			break;
		case AOUT_SOURCE_AUX0R:
			anactl &= ~DAC_ANACTL_OP0R2PAL;
			break;
		case AOUT_SOURCE_AUX1:
			anactl &= ~DAC_ANACTL_OP1L2DMTL;
			break;
		default:
			return;
		}
	}

	if (setting->par_src != AOUT_SOURCE_NONE) {
		switch (setting->par_src) {
		case AOUT_SOURCE_PCM_SINGLE:
		case AOUT_SOURCE_PCM_DOUBLE:
			anactl &= ~(DAC_ANACTL_DAENR | DAC_ANACTL_DPBMR);
			break;
		case AOUT_SOURCE_MIC1:
			anactl &= ~DAC_ANACTL_MIC12PAR;
			break;
		case AOUT_SOURCE_AUX0L:
			anactl &= ~DAC_ANACTL_OP0L2PAR;
			break;
		case AOUT_SOURCE_AUX0R:
			anactl &= ~DAC_ANACTL_OP0R2DMTR;
			break;
		case AOUT_SOURCE_AUX1:
			anactl &= ~DAC_ANACTL_OP1R2DMTR;
			break;
		default:
			return;
		}
	}

	dac->anactl = anactl;
}

void audio_out_enable(struct device *dev, dac_setting_t *setting)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac = cfg->dac;
	struct acts_audio_out_data *data = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_audio_out_enable__ENABLE_AUDIO_OUT, 0);

	if (atomic_inc(&data->enabled_cnt) == 0) {
		audio_out_init_clk(cfg, setting);

		/*
		 * fifo input from dma, fifo empty drq enable, channel select stereo
		 * dith enable, osr 32x, cic filter 4x ,enable mix
		 */
		dac->digctl = DAC_DIGCTL_ENDITH | DAC_DIGCTL_OSRDA_32X |
			      DAC_DIGCTL_INTFRS_4X | DAC_DIGCTL_DACHNUM_STEREO |
			      DAC_DIGCTL_DAFIS_DMA | DAC_DIGCTL_DAFEDE;

		/* clear fifo reset & enable dac */
		dac->digctl |= DAC_DIGCTL_DDEN;
		k_busy_wait(10);
		dac->digctl |= DAC_DIGCTL_DAFRT;

		/* clear pcm buf irq pending */
		dac->pcm_buf_stat = PCM_BUF_STAT_EPIP | PCM_BUF_STAT_FUIP |
				    PCM_BUF_STAT_HEIP | PCM_BUF_STAT_HFIP;

		/* enable pa bias */
		dac->anactl = DAC_ANACTL_BIASEN;
	}

	if (setting->dst_type != DAC_DST_I2S_TX) {
		/* enable dac channels */
		audio_out_enable_pa_source(dev, setting);

		/*enable palrw swing select 1.6vpp*/
		dac->bias = 0x335491;

		/* enable pa left channel */
		if (setting->pal_src != AOUT_SOURCE_NONE)
			dac->anactl |= DAC_ANACTL_PAENL | DAC_ANACTL_PAOSENL;

		/* enable pa right channel */
		if (setting->par_src != AOUT_SOURCE_NONE)
			dac->anactl |= DAC_ANACTL_PAENR | DAC_ANACTL_PAOSENR;

		if (setting->pal_src != AOUT_SOURCE_PCM_SINGLE && setting->par_src != AOUT_SOURCE_PCM_SINGLE)
			setting->mono = false;

		if (setting->mono)
			dac->digctl |= DAC_DIGCTL_DACHNUM_MONO;
		else if (setting->mixed)
			dac->digctl |= DAC_DIGCTL_DALRMIX;
	} else {
		cfg->i2s->txctl = 0x8; //32fs
	}

	acts_clock_audiout_set_rate(setting->sample_rate);

#ifdef BOARD_AUDIO_PA_EN_GPIO
	/* enable external PA */
	if (setting->dst_type != DAC_DST_I2S_TX && setting->pal_src != AOUT_SOURCE_NONE)
		gpio_pin_write(data->gpio_dev, BOARD_AUDIO_PA_EN_GPIO, 1);
#endif

	data->enabled_dst_type |= BIT(setting->dst_type);
}

void audio_out_disable(struct device *dev, dac_setting_t *setting)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_out_data *data = dev->driver_data;
	struct acts_audio_dac *dac = cfg->dac;

	if (setting->dst_type == DAC_DST_I2S_TX) {
		cfg->i2s->txctl = 0;
		acts_clock_peripheral_disable(cfg->i2s_clock_id);
	} else {
		audio_out_disable_pa_source(dev, setting);
	}

	data->enabled_dst_type &= ~BIT(setting->dst_type);

	if (atomic_dec(&data->enabled_cnt) != 1)
		return;

#ifdef BOARD_AUDIO_PA_EN_GPIO
	/* disable external PA */
	gpio_pin_write(data->gpio_dev, BOARD_AUDIO_PA_EN_GPIO, 0);
#endif

	/* disable audio DAC */
	dac->digctl = 0;
	dac->anactl = 0;

	/* disable audio DAC clock */
	acts_clock_peripheral_disable(cfg->clock_id);
}

void audio_out_set_pa(struct device *dev, bool enable)
{
#ifdef BOARD_AUDIO_PA_EN_GPIO
	struct acts_audio_out_data *data = dev->driver_data;
	if(data != NULL)
	{
		gpio_pin_write(data->gpio_dev, BOARD_AUDIO_PA_EN_GPIO, enable);
	}
#endif
}

int audio_out_init(struct device *dev)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_out_data *data = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_audio_out_init__INIT_AUDIO_OUTPUT, 0);

	data->dma_dev = device_get_binding(CONFIG_DMA_0_NAME);
	if (!data->dma_dev)
		return -ENODEV;

#ifdef BOARD_AUDIO_PA_EN_GPIO
	data->gpio_dev = device_get_binding(BOARD_AUDIO_PA_EN_GPIO_NAME);
	if (!data->gpio_dev) {
		SYS_LOG_ERR("cannot found GPIO dev: %s", BOARD_AUDIO_PA_EN_GPIO_NAME);
		return -1;
	}

	gpio_pin_configure(data->gpio_dev, BOARD_AUDIO_PA_EN_GPIO, GPIO_DIR_OUT);
#endif

	cfg->irq_config();

	/* dummy driver api to make device_get_binding() happy */
	dev->driver_api = (const void *)0x12345678;

	acts_reset_peripheral_deassert(cfg->reset_id);

	return 0;
}

static struct acts_audio_out_data audio_out_acts_data;
static void audio_out_acts_irq_config(void);

static const struct acts_audio_out_config audio_out_acts_config = {
	.dac = (struct acts_audio_dac *) AUDIO_DAC_REG_BASE,
	.i2s = (struct acts_audio_i2s *) AUDIO_I2S_REG_BASE,
	.i2s_clock_id = CLOCK_ID_I2S_TX,
	.clock_id = CLOCK_ID_AUDIO_DAC,
	.reset_id = RESET_ID_AUDIO,
	.dma_id = DMA_ID_AUDIO_I2S_TX_DAC,
	.dma_chan = CONFIG_AUDIO_OUT_ACTS_DMA_CHAN,
	.irq_config = audio_out_acts_irq_config,
};

DEVICE_INIT(audio_out, CONFIG_AUDIO_OUT_ACTS_DEV_NAME, audio_out_init,
	    &audio_out_acts_data, &audio_out_acts_config,
	    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

static void audio_out_acts_irq_config(void)
{
	IRQ_CONNECT(IRQ_ID_AUDIO_DAC_I2S_TX, 0, audio_out_pcmbuf_isr,
		    DEVICE_GET(audio_out), 0);
	irq_enable(IRQ_ID_AUDIO_DAC_I2S_TX);
}
