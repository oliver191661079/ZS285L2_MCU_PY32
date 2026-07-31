/***************************************************************************
 * Copyright (c) 2018 Actions Semi Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * Author: mikeyang<mikeyang@actions-semi.com>
 *
 * Description:   audio driver head file ( physical layer )
 *
 * Change log:
 *	2018/5/23: Created by mike.
 ****************************************************************************
 */

#ifndef __PHY_AUDIO_H__
#define __PHY_AUDIO_H__

#include <kernel.h>
#include <init.h>
#include <device.h>
#include <irq.h>
#include <soc.h>
#include <string.h>
#include <gpio.h>
#include <board.h>
#include <dma.h>
#include <cbuf.h>
#include <audio_common.h>

/* PCMBUF interrupt control flag */
#define PCMBUF_IP_HE		(1 << 0)
#define PCMBUF_IP_EP		(1 << 1)
#define PCMBUF_IP_HF		(1 << 2)
#define PCMBUF_IP_FU		(1 << 3)
typedef void (*pcmbuf_irq_cbk)(uint32_t pending);


/********************************************************************************************
*								audio device common enum
*********************************************************************************************
*/

/**
**	sample rate type
**/
typedef enum {
	SAMPLE_TYPE_44 = 0,
	SAMPLE_TYPE_48,
	SAMPLE_TYPE_MAX,
} sample_type_e;


/**
**	asrc ram index
**/
typedef enum {
	K_OUT0_P012                        = 0,              // 2K*2*24bit
	K_OUT0_P01,                                          // 2K*2*16bit
	K_OUT0_P0,                                           // 1K*2*16bit
	K_OUT0_U0,                                           // 0.5K*2*16bit
	K_OUT0_U0P5,                                         // 0.5K*2*24bit
	K_OUT0_MAX,
	K_OUT1_P2,                                           // 1K*2*16bit
	K_OUT1_P12_16bit,                                    // 2K*2*16bit
	K_OUT1_P3,                                           // 1K*2*16bit
	K_OUT1_P1,                                           // 1K*2*16bit
	K_OUT1_P12_24bit,                                    // 1K*2*24bit
	K_OUT1_MAX,
	K_IN_P12,                                            // 2K*2*16bit
	K_IN_P2,                                             // 1K*2*16bit
	K_IN_U1,                                             // 0.5K*2*16bit
	K_IN_U1P4,                                           // 0.5K*2*24bit
	K_IN0_MAX,
	K_IN1_P1,                                            // 1K*2*16bit
	K_IN1_P3,                                            // 1K*2*16bit
	K_IN1_P36,                                           // 1K*2*24bit
	K_ASRC_RAM_INDEX_MAX,
} asrc_ram_index_e;

/**
**	audiopll type
**/
typedef enum {
	AUDIOPLL_TYPE_0 = 0,
	AUDIOPLL_TYPE_1,
	AUDIOPLL_TYPE_MAX,
} audiopll_type_e;

/**
**	asrc type
**/
typedef enum {
    ASRC_OUT_NONE,
	ASRC_OUT_0,
	ASRC_OUT_1,
	ASRC_OUT_MAX,
	ASRC_IN_0,
	ASRC_IN_1,
	ASRC_TYPE_MAX,
} asrc_type_e;

/**
**	ASRC Level
**/
typedef enum {
	ASRC_LEVEL_0 = 0,
	ASRC_LEVEL_1,
	ASRC_LEVEL_2,
	ASRC_LEVEL_DEFAULT,
	ASRC_LEVEL_4,
	ASRC_LEVEL_5,
	ASRC_LEVEL_6,
	ASRC_LEVEL_7,
	ASRC_MAX_LEVEL,
    ASRC_LEVEL_SLOWEST = 0xE0,
	ASRC_LEVEL_SLOWER ,
	ASRC_LEVEL_FASTER,
	ASRC_LEVEL_FASTEST,
} asrc_level_e;

/**
**	clock type
**/
typedef enum {
	CLOCK_TYPE_256FS = 0,
	CLOCK_TYPE_128FS,
	CLOCK_TYPE_MAX,
} clock_type_e;

/**
**	i2s device role enum
**/
typedef enum {
	I2S_DEVICE_MASTER = 0,
	I2S_DEVICE_SLAVE,
} i2s_device_role_e;

/**
**	i2s format type enum
**/
typedef enum {
	I2S_FORMAT_I2S = 0,
	I2S_FORMAT_LEFT,
	I2S_FORMAT_RIGHT,
} i2s_fmt_type_e;

/**
**	i2s BCLK Rate enum
**/
typedef enum {
	I2S_BCLK_64FS = 0,				/* bclk = 64fs,  mclk = 256fs */
	I2S_BCLK_32FS,					/* bclk = 32fs,  mclk = 128fs */
} i2s_bclk_rate_e;

/********************************************************************************************
*								aout device enum
*********************************************************************************************
*/

/**
**	a-a channel data source
**/
typedef enum {
	AA_SOURCE_NONE = 0,
	AA_SOURCE_AUX0,
	AA_SOURCE_AUX1,
	AA_SOURCE_AUX2,
	AA_SOURCE_AUXFD,
	AA_SOURCE_ASEMIC,
	AA_SOURCE_AFDMIC,
	AA_SOURCE_MAX,
} aa_source_e;

/**
**  audio out fifo source,   the fifo can be write by cpu/dma/asrc/dsp....
**/
typedef enum {
	AOUT_FIFO_SRC_CPU  = 0,
	AOUT_FIFO_SRC_DMA  = 1,
	AOUT_FIFO_SRC_ASRC = 2,
	AOUT_FIFO_SRC_DSP  = 3,
} aout_fifo_src_e;

typedef enum {
	AUDIO_MODE_DEFAULT = 0,
	AUDIO_MODE_MONO,                    //mono->left mono->right
	AUDIO_MODE_STEREO,                  //left->left right->right
    AUDIO_MODE_LEFT,                    //all left
    AUDIO_MODE_RIGHT,                   //all right
    AUDIO_MODE_LEFT_MUTE_RIGHT,         //left->mute right->right
    AUDIO_MODE_RIGHT_MUTE_LEFT,         //left->left right->mute
    AUDIO_MODE_LEFT_MONO_OTHER_MUTE,    //left->mono right->mute
    AUDIO_MODE_RIGHT_MONO_OTHER_MUTE,   //right->mono left->mute
    AUDIO_MODE_LEFT_RIGHT_MIX,          //left and right mix-->mono
    AUDIO_MODE_LEFT_RIGHT_SEPERATE,
} audio_mode_e;

/**
**	audio out channel type
**/
typedef enum {
	AOUT_CHANNEL_AA = 0,					/* A-A 通路*/
	AOUT_CHANNEL_DAC,						/* DAC 独立通路*/
	AOUT_CHANNEL_I2S,						/* I2S 独立通路*/
	AOUT_CHANNEL_SPDIF,						/* SPDIF 独立通路*/
	AOUT_CHANNEL_LINKAGE_DAC_I2S,			/* DAC-I2S 联动通路*/
	AOUT_CHANNEL_LINKAGE_DAC_SPDIF,			/* DAC-SPDIF 联动通路*/
	AOUT_CHANNEL_LINKAGE_DAC_I2S_SPDIF,		/* DAC-I2S-SPDIF 联动通路*/
	AOUT_CHANNEL_LINKAGE_I2S_SPDIF,			/* I2S-SPDIF 联动通路*/
	AOUT_CHANNEL_USPEAKER,
} aout_channel_type_e;


/**
**	audio out device clock select
**/
typedef enum {
	AOUT_DAC_CLK_DAC256FS = 0,				/* DAC 设备时钟:    DAC   256FS  */
	AOUT_I2S_CLK_DAC256FS = 0x10,			/* I2S 设备时钟:  DAC 256FS */
	AOUT_I2S_CLK_ADCCLK   = 0x11,			/* I2S 设备时钟:  ADC CLK */
	AOUT_I2S_CLK_I2S0CLK  = 0x12,			/* I2S 设备时钟:  I2S0 CLK */
	AOUT_I2S_CLK_I2STX0MCLK_EXT = 0x13,		/* I2S 设备时钟:  I2STX0 MCLK EXT */
	AOUT_I2S_CLK_DAC128FS = 0x14,			/* I2S 设备时钟:  DAC 128FS */
	AOUT_SDF_CLK_DAC128FS = 0x20,			/* SPDIF 设备时钟:  DAC 128FS */
	AOUT_SDF_CLK_I2S0CLK  = 0x21,			/* SPDIF 设备时钟:  I2S0 CLK */
	AOUT_SDF_CLK_I2S0CLK_DIV2   = 0x22,		/* SPDIF 设备时钟:  I2S0 CLK /2 */
	AOUT_DEV_CLK_MAX,
} aout_device_clk_e;

/**
**	index for get aout channel node info
**/
typedef enum {
	GET_AOUT_FIFO_USING = 0,
	GET_AOUT_FIFO_SRC,
	GET_AOUT_ASRC_USING,
	GET_AOUT_FIFO_STATUS,
	GET_AOUT_INF_MAX,
} aout_channel_info_e;

/**
**	cmd for pa control
**/
typedef enum {
	PA_OPEN = 0,
	PA_CLOSE,
	PA_GET_STATUS,
	PA_SET_VOLUME,
	PA_GET_VOLUME,
	PA_MUTE_CTL,
} aout_pa_ctl_e;


/********************************************************************************************
*								ain device enum
*********************************************************************************************
*/

/**
**  audio in fifo enum
**/
typedef enum {
	AIN_FIFO_NOUSE = 0,
	AIN_FIFO_ADC,
	AIN_FIFO_I2SRX1,
	AIN_FIFO_MAX,
} ain_fifo_e;

/**
**	audio in fifo destination
**/
typedef enum {
	AIN_FIFO_DST_CPU = 0,
	AIN_FIFO_DST_DMA,
	AIN_FIFO_DST_ASRC,
	AIN_FIFO_DST_DSP,
} ain_fifo_dst_e;

/**
**	audio in channel type
**/
typedef enum {
	AIN_CHANNEL_AA = 0,
	AIN_CHANNEL_ADC,
	AIN_CHANNEL_DMIC,
	AIN_CHANNEL_I2SRX0,
	AIN_CHANNEL_I2SRX1,
	AIN_CHANNEL_SPDIF,
	AIN_CHANNEL_USB,
} ain_channel_type_e;

/**
**	audio in device clock select
**/
typedef enum {
	AIN_ADC_CLK_ADCCLK = 0,			/* ADC 设备时钟:    ADC Clock */
	AIN_I2S0_CLK_ADCCLK = 0x10,		/* I2S RX0 设备时钟:   ADC Clock */
	AIN_I2S0_CLK_I2STX0MCLK,		/* I2S RX0 设备时钟:   I2STX0 MCLK */
	AIN_I2S0_CLK_I2S1CLK,			/* I2S RX0 设备时钟:   I2S1 CLK */
	AIN_I2S0_CLK_I2SRX0MCLKEXT,		/* I2S RX0 设备时钟:   I2SRX0 MCLK EXT */
	AIN_I2S1_CLK_I2S1CLK = 0x20,	/* I2S RX1 设备时钟:   I2S1 CLK */
	AIN_I2S1_CLK_I2STX0MCLK,		/* I2S RX1 设备时钟:   I2STX0 MCLK */
	AIN_I2S1_CLK_I2SRX0MCLK,		/* I2S RX1 设备时钟:   I2SRX0 MCLK */
	AIN_I2S1_CLK_I2SRX1MCLKEXT,		/* I2S RX1 设备时钟:   I2SRX1 MCLK EXT */
	AIN_SDF_CLK_APLL0 = 0x30,		/* SPDIFRX 设备时钟:  audiopll 0 */
	AIN_SDF_CLK_APLL1,				/* SPDIFRX 设备时钟:  audiopll 1 */
	AIN_SDF_CLK_COREPLL,			/* SPDIFRX 设备时钟:  corepll */
	AIN_DEV_CLK_MAX,
} ain_device_clk_e;

/**
**	index for get ain channel node info
**/
typedef enum {
	GET_AIN_FIFO_USING = 0,
	GET_AIN_FIFO_DST,
	GET_AIN_ASRC_USING,
	GET_AIN_SAMPLERATE_STATUS,
	GET_AIN_SAMPLERATE_VALUE,
	GET_AIN_INF_MAX,

} ain_channel_info_e;


/**
**	audio in sample rate status
**/
typedef enum {
	RX_SR_STATUS_NOTGET = 0,					/* 尚未获取到采样率*/
	RX_SR_STATUS_CHANGE,						/* 检测到采样率发生了变化*/
	RX_SR_STATUS_GET,							/* 获取到了采样率*/

} rx_sr_status_e;

/**
**	audio pll alloc method
**/
typedef enum {
    /* 自动分配使用的audiopll，默认方式 */
    APLL_ALLOC_AUTO = 0,

    APLL_ALLOC_AUDIOPLL0 = 1,

    APLL_ALLOC_AUDIOPLL1 = 2,
} apll_alloc_method_e;


/********************************************************************************************
*								audio device structure
*********************************************************************************************
*/

/**
**	PA mute setting
**/
typedef struct {
	uint8_t left_mute_flag:1;
	uint8_t right_mute_flag:1;
} pa_mute_setting_t;

/**
**	PA Setting Param
**/
typedef struct {
	uint8_t left_mute_flag:1;          /* 0: not mute;    1:  mute */
	uint8_t right_mute_flag:1;         /* 0: not mute;    1:  mute */
	uint8_t pa_reserved0:6;
	ain_logic_source_type_e aa_mode_logic_src;     /* a-a 传输模式下的数据来源， 非a-a  模式时，请配置为0  */
	aa_source_e aa_mode_src;           /* a-a 传输模式下实际的物理输入源，由hal  根据aa_mode_logic_src  填充*/
} phy_pa_setting_t;


/**
**	DAC Setting Param
**/
typedef struct {
	uint8_t dac_sample_rate;					/* DAC  采样率*/
	uint8_t sample_cnt_enable;					/* 是否使能fifo sample count */
	uint32_t dac_gain;                          /* DAC  增益参数，byte 0:  da volume;    byte 1:  pa volume */
} phy_dac_setting_t;

/**
**	I2S TX Setting Param
**/
typedef struct {
	uint8_t i2stx_sample_rate;					/* I2S TX 采样率KHz */
	i2s_device_role_e i2stx_role;               /* I2STX 设备角色*/
	i2s_fmt_type_e i2stx_format;				/* I2STX 格式*/
	int (*srd_callback)(void *cb_data, uint32_t cmd, void *param);
	void *cb_data;
} phy_i2stx_setting_t;

/**
**	SPDIF TX Setting Param
**/
typedef struct {
	uint8_t spdiftx_sample_rate;				/* spdiftx 采样率*/
} phy_spdiftx_setting_t;

/**
**	I2S RX Setting Param
**/
typedef struct {
	uint8_t i2srx_sample_rate;					/* I2SRX 采样率KHz */
	i2s_device_role_e i2srx_role;               /* I2SRX 设备角色*/
	i2s_fmt_type_e i2srx_format;				/* I2SRX 格式*/
	int (*srd_callback)(void *cb_data, uint32_t cmd, void *param);
	void *cb_data;
} phy_i2srx_setting_t;


/**
**	SPDIF RX Setting Param
**/
typedef struct {
	int (*spdifrx_callback)(void *cb_data, uint32_t cmd, void *param);	/* 采样率变化后回调函数*/
	void *callback_data;						/* 回调传参*/
} phy_spdifrx_setting_t;

/**
**	ASRC Setting Param
**/
typedef struct
{
	uint32_t ctl_wr_clk;
	uint32_t ctl_rd_clk;
	uint32_t ctl_mode;
	uint32_t ctl_dma_width;
	uint32_t input_samplerate;
	uint32_t output_samplerate;
	uint32_t reg_dec0;
	uint32_t reg_dec1;
	uint32_t reg_hfull;
	uint32_t reg_hempty;
	uint32_t by_pass;
	asrc_ram_index_e asrc_ram;
} phy_asrc_param_t;

/**
**  audio dma Setting Param
**/
typedef struct {
	uint8_t *dma_src_buf;                       /* dma 源buffer  地址*/
	uint8_t *dma_dst_buf;						/* dma 目标buffer 地址*/
	uint32_t dma_trans_len;                     /* dma 传输长度*/
	uint8_t dma_width;                          /* dma 传输宽度, unit: bytes */
	uint8_t dma_reload;                         /* 是否启用reload 模式*/
	uint8_t dma_interleaved;                    /* 是否用interleaved 模式*/
	uint8_t dma_burst_len;                      /* burst传输长度 */
	uint8_t dma_sam;                            /* source address costant */
	dma_stream_handle_t stream_handle;          /* 回调函数*/
	void *stream_pdata;                         /* 回调函数传参*/
	uint8_t all_complete_callback_en;           /* 全传完成回调*/
	uint8_t half_complete_callback_en;		    /* 半传完成回调*/
} dma_setting_t;


/**
**  audio out  通路物理层参数
**/
typedef struct {
	uint8_t need_dma;							/* 是否使用DMA  */
	uint8_t need_dma_reload;                    /* 是否使用DMA reload模式 */
	uint8_t data_mode;                          /* 单声道or立体声数据 */
	uint8_t asrc_alloc_method;
	int8_t channel_apll_sel;
	aout_channel_type_e aout_channel_type;      /* aout 通路类型*/
	audio_outfifo_sel_e aout_fifo_sel;					/* aout 通路使用的硬件FIFO */
	aout_fifo_src_e aout_fifo_src_type;			/* 硬件FIFO  的输入源*/

	phy_pa_setting_t *ptr_pa_setting;				/* pa 参数，需要时传入*/
	phy_dac_setting_t *ptr_dac_setting;				/* dac 参数，需要时传入*/
	phy_i2stx_setting_t *ptr_i2stx_setting;			/* i2s tx 参数，需要时传入*/
	phy_spdiftx_setting_t *ptr_spdiftx_setting; 	/* spdif tx 参数，需要时传入*/
	phy_asrc_param_t *ptr_asrc_setting;			/* asrc 参数，需要时传入  */
} PHY_audio_out_param_t;


/**
**	audio out 通道句柄
**/
typedef struct {
	uint16_t channel_type;							  /* 通路类型*/
	aa_source_e aa_channel_source;					  /* a-a 通路数据源*/

	audio_outfifo_sel_e  channel_fifo_sel;                    /* 通路所使用的FIFO */
	aout_fifo_src_e channel_fifo_src;                 /* 通路FIFO  的输入源*/
	uint8_t  fifo_cnt_enable;                         /* 通路fifo count 功能是否启用*/
	uint32_t changed_sample_cnt;					  /* 通过asrc 插入或者是删掉的samples 数目*/

	uint8_t input_sample_rate;					 	  /* 通路输入采样率， 以KHz  为单位*/
	uint8_t channel_sample_rate;					  /* 通路采样率， 以KHz  为单位*/
	sample_type_e channel_sample_type;				  /* 通路采样率系列*/
	audiopll_type_e  channel_apll_sel;                /* 通路所用的audiopll */
	clock_type_e  channel_clk_type;                   /* 通路设备时钟类型*/
	aout_device_clk_e channel_dev_clk;	              /* 通路设备时钟源*/

	asrc_type_e channel_asrc_index;                   /* 通路所用asrc  ，未使用时，为ASRC_OUT_MAX */
	uint32_t channel_asrc_dec;						  /* 通路asrc 的dec  值,    默认dec1  和dec0  一致*/

	int aout_dma;									  /* 通路如使用DMA， 记录dma index，否则，为-1 */
	int phy_dma;

	uint8_t *dma_buf_ptr;
	uint8_t dma_reload;
	uint8_t dma_width;                                /* 2: dma width 16bits; 4: dma width 32bits */
	bool dma_run_flag;								  /* 通路ringbuf  dma  启动标记*/
	uint8_t data_mode;
	uint16_t dma_buf_size;
	uint16_t dma_first_buf_size;
	int (*callback)(void *cb_data, uint32_t reason);
	void *callback_data;
    uint8_t sr_input;
    uint8_t sr_output;
} aout_channel_node_t;


/**
**	ain gain
**/
typedef struct {
    uint32_t inputsrc_gain;
    uint32_t adc_gain;
} ain_gain_t;

/**
**	input setting Param
**/
typedef struct {
	uint16_t input_gain;						/* audio in 输入源增益*/
	uint16_t boost;
	ain_track_e ain_track;						/* audio in 输入源声道选择*/
	uint8_t input_gain_force_0db : 1;			/* MIC input gain force 0db */
} input_setting_t;


/**
**	ADC Setting Param
**/
typedef struct {
	uint8_t adc_sample_rate;					/* ADC  采样率*/
	uint32_t adc_gain;                          /* ADC  增益*/
} phy_adc_setting_t;

/**
**  audio in 物理层参数
**/
typedef struct {
	uint8_t need_dma;							/* 是否需使用DMA */
	uint8_t need_dma_reload;
	uint8_t data_mode;
	uint8_t asrc_alloc_method;
	int8_t channel_apll_sel;
	ain_channel_type_e ain_channel_type;      	/* audio in 通路类型*/

	ain_source_type_e ain_src;					/* audio in 输入源*/

	ain_fifo_e ain_fifo_sel;                    /* audio in 输入FIFO 选择*/
	ain_fifo_dst_e ain_fifo_dst;                /* audio in FIFO 的输出*/

	input_setting_t *ptr_input_setting;			/* audio in input 参数，如不使用，为NULL */
	phy_adc_setting_t *ptr_adc_setting;				/* adc 配置参数，如不使用， 为NULL  */
	phy_i2srx_setting_t *ptr_i2srx_setting;			/* i2s rx 配置参数，如不使用，为NULL  */
	phy_spdifrx_setting_t *ptr_spdifrx_setting;		/* spdif rx 配置参数，如不使用，为NULL*/
	phy_asrc_param_t *ptr_asrc_setting;			/* asrc 配置参数， 如不使用， 为NULL  */

}PHY_audio_in_param_t;


/**
**	audio in channel handle
**/
typedef struct {
	ain_channel_type_e channel_type;				  /* 通路类型*/

	ain_source_type_e channel_src_type;               /* 通路输入设备类型*/
	ain_fifo_e channel_fifo_sel;                     /* 通路所使用的FIFO */
	ain_fifo_dst_e channel_fifo_dst;                  /* 通路FIFO  的输出*/

	uint8_t channel_sample_rate;					  /* 通路采样率*/
	sample_type_e channel_sample_type;				  /* 通路采样率系列*/
	rx_sr_status_e channel_sample_status;			  /* 通路采样率状态*/
	audiopll_type_e  channel_apll_sel;                /* 通路所用的audiopll */
	clock_type_e  channel_clk_type;                   /* 通路设备时钟类型*/
	ain_device_clk_e channel_dev_clk;	              /* 通路设备时钟源*/

	asrc_type_e channel_asrc_index;                   /* 通路所用asrc  ，未使用时，为ASRC_MAX */
	uint32_t channel_asrc_dec;						  /* 通路asrc 的dec  值，默认dec0 和dec1 一致*/

	int ain_dma;									  /* 通路如使用DMA， 记录dma index，否则，为-1 */

	int (*sr_change_callback)(void *cb_data, uint32_t cmd, void *param);		  /* 采样率变化后回调函数*/
	void *spdif_callback_data;

	uint8_t *dma_buf_ptr;
	uint8_t dma_reload;
	bool dma_run_flag;								  /* 通路ringbuf  dma  启动标记*/
	uint8_t dma_width;
	uint8_t data_mode;
	uint32_t dma_buf_size;
	int (*callback)(void *cb_data, uint32_t reason);
	void *callback_data;

    uint8_t sr_input;
    uint8_t sr_output;
} ain_channel_node_t;

extern uint32_t get_sample_rate_hz(uint8_t fs_khz);

extern uint32_t get_sample_pairs(uint8_t sample_hz);

extern uint32_t get_sample_rate_khz(uint32_t fs_hz);

extern uint8_t dac_volume_db_to_level(int32_t vol);

extern int32_t dac_volume_level_to_db(uint8_t level);

extern int adc_dmic_gain_translate(int16_t gain, uint16_t *input_gain, uint16_t *dig_gain);

extern int adc_amic_gain_translate(int16_t gain, uint16_t *input_gain, uint16_t *dig_gain);

extern int adc_aux_se_gain_translate(int16_t gain, uint16_t *input_gain, uint16_t *dig_gain);

extern int adc_aux_fd_gain_translate(int16_t gain, uint16_t *input_gain, uint16_t *dig_gain);

#endif

