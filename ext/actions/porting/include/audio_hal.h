/*
 * Copyright (c) 2020 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Audio HAL
 */


#ifndef __AUDIO_HAL_H__
#define __AUDIO_HAL_H__
#include <os_common_api.h>
#include <assert.h>
#include <string.h>
#include <audio_out.h>
#include <audio_in.h>
//#include <dma.h>

/**
**  audio out hal context struct
**/
typedef struct {
	struct  device *aout_dev;
} hal_audio_out_context_t;

/**
**  audio in hal context struct
**/
typedef struct {
	struct  device *ain_dev;
} hal_audio_in_context_t;

#define AUDIO_CHANNEL_DAC               (1 << 0)
#define AUDIO_CHANNEL_I2STX             (1 << 1)
#define AUDIO_CHANNEL_SPDIFTX           (1 << 2)
#define AUDIO_CHANNEL_ADC               (1 << 3)
#define AUDIO_CHANNEL_I2SRX             (1 << 4)
#define AUDIO_CHANNEL_SPDIFRX           (1 << 5)
#define AUDIO_CHANNEL_AA                (1 << 6)
#define AUDIO_CHANNEL_I2SRX1            (1 << 7)

#define	APS_LEVEL_1  			0
#define	APS_LEVEL_2  			1
#define	APS_LEVEL_3  			2
#define	APS_LEVEL_4  			3
#define	APS_LEVEL_5  			4
#define	APS_LEVEL_6  			5
#define	APS_LEVEL_7  			6
#define	APS_LEVEL_8  			7

#define AOUT_DMA_IRQ_HF         (1 << 0)     /*!< DMA irq half full flag */
#define AOUT_DMA_IRQ_TC         (1 << 1)     /*!< DMA irq transfer completly flag */

#define	AOUT_FIFO_DAC0 			0
#define AOUT_FIFO_DAC1			1
#define AOUT_FIFO_I2STX0    	2
#define	AUDIO_INPUT_DEV_NONE   	0xff

typedef struct {
	uint32_t ctl_mode;                          /* 0: src mode;    1: asrc mode */
	uint32_t input_sr;
	uint32_t output_sr;
	uint32_t by_pass;
	uint32_t data_width;
} asrc_setting_t;

/**
 *  audio out init param
 **/
typedef struct {
	u8_t aa_mode:1;
	u8_t need_dma:1;
	u8_t dma_reload:1;
	u8_t out_to_pa:1;
	u8_t out_to_i2s:1;
	u8_t out_to_spdif:1;
	u8_t sample_cnt_enable:1;

	u8_t sample_rate;
	u8_t channel_type;
	u8_t channel_id;
	u8_t channel_mode;
	u8_t data_width;
	u16_t reload_len;
	u8_t *reload_addr;
	int left_volume;
	int right_volume;

	asrc_setting_t *asrc_setting;

	int (*callback)(void *cb_data, u32_t reason);
	void *callback_data;
}audio_out_init_param_t;


/**
**  audio in init param
**/
typedef struct {
	uint8_t aa_mode:1;
	uint8_t need_dma:1;
	uint8_t need_asrc:1;
	uint8_t need_dsp:1;
	uint8_t reserved_1:1;
	uint8_t dma_reload:1;
	uint8_t data_mode;

	u8_t sample_rate;
	u8_t channel_type;
	u16_t audio_device;
	u8_t data_width;
	s16_t adc_gain;
	s16_t input_gain;
	u8_t boost_gain:1;

	u16_t reload_len;
	u8_t *reload_addr;

	int (*callback)(void *cb_data, u32_t reason);
	void *callback_data;
}audio_in_init_param_t;

int hal_audio_out_init(void);
void* hal_aout_channel_open(audio_out_init_param_t *init_param);
int hal_aout_channel_start(void* aout_channel_handle);
int hal_aout_channel_prepare_start(void *aout_channel_handle);
int hal_aout_channel_write_data(void* aout_channel_handle, u8_t *data, u32_t data_size);
int hal_aout_channel_stop(void* aout_channel_handle);
int hal_aout_channel_close(void* aout_channel_handle);
int hal_aout_channel_set_aps(void *aout_channel_handle, unsigned int aps_level, unsigned int aps_mode);
u32_t hal_aout_channel_get_sample_cnt(void *aout_channel_handle);
int hal_aout_channel_reset_sample_cnt(void *aout_channel_handle);
int hal_aout_channel_enable_sample_cnt(void *aout_channel_handle, bool enable);
int hal_aout_channel_check_fifo_underflow(void *aout_channel_handle);
int hal_aout_channel_mute_ctl(void *aout_channel_handle, u8_t mode);
int hal_aout_channel_set_pa_vol_level(void *aout_channel_handle, int vol_level);
int hal_aout_set_pcm_threshold(void *aout_channel_handle, int he_thres, int hf_thres);
int hal_aout_open_pa(int antipop_enable);
int hal_aout_close_pa(int antipop_enable);
int hal_aout_pa_class_select(u8_t pa_mode);


int hal_audio_in_init(void);
void* hal_ain_channel_open(audio_in_init_param_t *init_param);
int hal_ain_channel_start(void* ain_channel_handle);
int hal_ain_channel_read_data(void* ain_channel_handle, u8_t *data, u32_t data_size);
int hal_ain_channel_stop(void* ain_channel_handle);
int hal_ain_channel_close(void* ain_channel_handle);
int hal_ain_channel_set_volume(void* ain_channel_handle, adc_gain *adc_volume);
typedef int (*srd_callback)(void *cb_data, u32_t cmd, void *param);
int hal_ain_set_contrl_callback(u8_t channel_type, srd_callback callback);
int hal_ain_channel_set_aps(void* ain_channel_handle, unsigned int aps_level, unsigned int aps_mode);
int hal_ain_channel_set_interleave_mode(void* ain_channel_handle, unsigned int interleave_mode);

#endif

