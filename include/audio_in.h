/*
 * Copyright (c) 2020 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for AUDIO IN drivers
 */

#ifndef __AUDIO_IN_H__
#define __AUDIO_IN_H__

#include <zephyr/types.h>
#include <stddef.h>
#include <device.h>
#include <board.h>
#include <audio_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AUDIO IN Interface
 * @addtogroup AUDIO_IN
 * @{
 */

/* The flag to indicate that the commands relate to ASRC */
#define AIN_ASRC_CMD_FLAG                                    (1 << 7)

/*! @brief Definition for the audio in control commands */
#define AIN_CMD_SET_ADC_GAIN                                  (1)
/*!< Set the ADC left channel gain value which in dB format.
 * int audio_in_control(dev, handle, #AIN_CMD_SET_ADC_GAIN, adc_gain *gain)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_SPDIF_GET_CHANNEL_STATUS                      (2)
/* Get the SPDIFRX channel status
 * int audio_in_control(dev, handle, #AIN_CMD_SPDIF_GET_CHANNEL_STATUS, audio_spdif_ch_status_t *sts)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_SPDIF_IS_PCM_STREAM                           (3)
/* Check if the stream that received from spdifrx is the pcm format
 * int audio_in_control(dev, handle, #AIN_CMD_SPDIF_IS_PCM_STREAM, bool *is_pcm)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_SPDIF_CHECK_DECODE_ERR                        (4)
/* Check if there is spdif decode error happened
 * int audio_in_control(dev, handle, #AIN_CMD_SPDIF_CHECK_DECODE_ERR, bool *is_err)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_I2SRX_QUERY_SAMPLE_RATE                       (5)
/* Query the i2c master device sample rate and i2srx works in slave mode.
 * int audio_in_control(dev, handle, #AIN_CMD_I2SRX_QUERY_SAMPLE_RATE, audio_sr_sel_e *is_err)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_GET_SAMPLERATE                                (6)
/*!< Get the channel audio sample rate by specified the audio channel handler.
 * int audio_in_control(dev, handle, #AIN_CMD_GET_SAMPLERATE, audio_sr_sel_e *sr)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_SET_SAMPLERATE                                (7)
/*!< Set the channel audio sample rate by the giving audio channel handler.
 * int audio_in_control(dev, handle, #AIN_CMD_SET_SAMPLERATE, audio_sr_sel_e *sr)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_GET_APS                                       (8)
/*!< Get the AUDIO_PLL APS
 * int audio_in_control(dev, handle, #AIN_CMD_GET_APS, audio_aps_level_e *aps)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_SET_APS                                       (9)
/*!< Set the AUDIO_PLL APS for the sample rate tuning
 * int audio_in_control(dev, handle, #AIN_CMD_SET_APS, audio_aps_level_e *aps)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_SET_PREPARE_START                             (10)
/*!< Set the flag of notifing to prepare start channel which means  prepare the DMA resources but not start yet.
 * int audio_in_control(dev, handle, #AIN_CMD_SET_PREPARE_START, NULL)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_GET_CH_INFO                                   (11)
/*!< Get the channel infomation
 * int audio_in_control(dev, handle, #AIN_CMD_GET_CH_INFO, uin8_t *index_result)
 * For #index_result, the io input is index and output is result.
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_ASRC_ENABLE                                  (AIN_ASRC_CMD_FLAG | 12)
/*!< Enable audio in channel output data to ASRC module.
 * int audio_out_control(dev, handle, #AOUT_CMD_ASRC_ENABLE, asrc_in_setting_t *setting)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_SET_ASRC_IN_SAMPLE_RATE                      (AIN_ASRC_CMD_FLAG | 13)
/*!< Set the ASRC input sample rate.
 * int audio_out_control(dev, handle, #AIN_CMD_SET_ASRC_IN_SAMPLE_RATE,  asrc_sample_rate_t *setting)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_RESET_ASRC_INFO                              (AIN_ASRC_CMD_FLAG | 14)
/*!< Reset the ASRC infomation.
 * int audio_out_control(dev, handle, #AIN_CMD_RESET_ASRC_INFO,  NULL)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_SET_INTERLEAVED_MODE                         (15)
/*!< Reset the ASRC infomation.
 * int audio_out_control(dev, handle, #AIN_CMD_SET_INTERLEAVED_MODE,  audio_interleave_mode_e *mode)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_DSP_ENABLE                                   (16)
/*!< Enable audio in channel output data to DSP module.
 * int audio_out_control(dev, handle, #AIN_CMD_DSP_ENABLE, NULL)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_ADC_AEC_GAIN                                 (17)
/*!< Set the ADC AEC channel gain value which in (dB x 10) format.
 * int audio_in_control(dev, handle, #AIN_CMD_ADC_AEC_GAIN, int16_t *gain)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_GET_ASRC_APS                                 (18)
/*!< Get the ASRC APS
 * int audio_out_control(dev, handle, #AIN_CMD_GET_ASRC_APS, asrc_level_e *aps)
 * Returns 0 if successful and negative errno code if error.
 */

#define AIN_CMD_SET_ASRC_APS                                 (19)
/*!< Set the ASRC APS for the sample rate tuning
 * int audio_out_control(dev, handle, #AIN_CMD_SET_ASRC_APS, asrc_level_e *aps)
 * Returns 0 if successful and negative errno code if error.
 */

/*!
 * struct asrc_in_setting_t
 * @brief The ASRC in setting parameters
 */
typedef struct {
	audio_asrc_mode_e mode;
		/*!< Specify the ASRC working mode */
	audio_asrc_mem_policy_e mem_policy;
		/*!< Specify the ASRC memory allocation policy */
	audio_asrc_in_rclk_e rclk;
		/*!< Specify the ASRC read clock  */
	uint8_t output_samplerate;
		/*!< output sample rate */
} asrc_in_setting_t;

/*!
 * struct adc_gain
 * @brief The ADC gain setting
 */
typedef struct {
#define ADC_GAIN_INVALID (0xFFFF)
	int16_t ch_gain[ADC_CH_NUM_MAX];
		/*!< The gain value shall be set by 10 multiple of actual value e.g ch_gain[0]=100 means channel 0 gain is 10 db;
		 * If gain value equal to #ADC_GAIN_INVALID that the setting will be ignored.
		 */
} adc_gain;

/*!
 * struct adc_setting_t
 * @brief The ADC setting parameters
 */
typedef struct {
	uint16_t device;
		/*!< ADC input device chooses */
	adc_gain gain;
		/*!< ADC gain setting */
} adc_setting_t;

/*!
 * struct i2srx_setting_t
 * @brief The I2SRX setting parameters
 */
typedef struct {
#define I2SRX_SRD_FS_CHANGE  (1 << 0)
	/*!< I2SRX SRD(sample rate detect) captures the event that the sample rate has changed
	 * int callback(cb_data, #I2SRX_SRD_FS_CHANGE, audio_sr_sel_e *sr)
	 */
#define I2SRX_SRD_WL_CHANGE  (1 << 1)
	/*!< I2SRX SRD(sample rate detect) captures the event that the effective width length has changed
	 * int callback(cb_data, #I2SRX_SRD_WL_CHANGE, audio_i2s_srd_wl_e *wl)
	 */
#define I2SRX_SRD_TIMEOUT    (1 << 2)
	/*!< I2SRX SRD(sample rate detect) captures the timeout (disconnection) event
	 * int callback(cb_data, #I2SRX_SRD_TIMEOUT, NULL)
	 */
	int (*srd_callback)(void *cb_data, uint32_t cmd, void *param);
		/*!< The callback function from I2SRX SRD module which worked in the slave mode */
	void *cb_data;
		/*!< Callback user data */
} i2srx_setting_t;

/*!
 * struct spdifrx_setting_t
 * @brief The SPDIFRX setting parameters
 */
typedef struct {
#define SPDIFRX_SRD_FS_CHANGE  (1 << 0)
	/*!< SPDIFRX SRD(sample rate detect) captures the event that the sample rate has changed.
	 * int callback(cb_data, #SPDIFRX_SRD_FS_CHANGE, audio_sr_sel_e *sr)
	 */
#define SPDIFRX_SRD_TIMEOUT    (1 << 1)
	/*!< SPDIFRX SRD(sample rate detect) timeout (disconnect) event.
	 * int callback(cb_data, #SPDIFRX_SRD_TIMEOUT, NULL)
	 */
	int (*srd_callback)(void *cb_data, uint32_t cmd, void *param);
		/*!< sample rate detect callback */
	void *cb_data;
		/*!< callback user data */
} spdifrx_setting_t;

/*!
 * struct ain_param_t
 * @brief The audio in configuration parameters
 */
typedef struct {
#define AIN_DMA_IRQ_HF (1 << 0)
	/*!< DMA irq half full flag */
#define AIN_DMA_IRQ_TC (1 << 1)
	/*!< DMA irq transfer completly flag */
	uint8_t sample_rate;
		/*!< The sample rate setting refer to enum audio_sr_sel_e */
	uint16_t channel_type;
		/*!< Indicates the channel type selection and can refer to #AUDIO_CHANNEL_ADC, #AUDIO_CHANNEL_I2SRX, #AUDIO_CHANNEL_SPDIFRX*/
	audio_ch_width_e channel_width;
		/*!< The channel effective data width */
	adc_setting_t *adc_setting;
		/*!< The ADC function setting if has */
	i2srx_setting_t *i2srx_setting;
		/*!< The I2SRX function setting if has */
	spdifrx_setting_t *spdifrx_setting;
		/*!< The SPDIFRX function setting if has  */
	int (*callback)(void *cb_data, uint32_t reason);
		/*!< The callback function which called when #AIN_DMA_IRQ_HF or #AIN_DMA_IRQ_TC events happened */
	void *cb_data;
		/*!< callback user data */
	audio_reload_t reload_setting;
		/*!< The reload mode setting which is mandatory*/
} ain_param_t;

/*!
 * struct ain_driver_api
 * @brief Public API for audio in driver
 */
struct ain_driver_api {
	void* (*ain_open)(struct device *dev, ain_param_t *param);
	int (*ain_close)(struct device *dev, void *handle);
	int (*ain_start)(struct device *dev, void *handle);
	int (*ain_stop)(struct device *dev, void *handle);
	int (*ain_control)(struct device *dev, void *handle, int cmd, void *param);
};

/*!
 * @brief Open the audio input channel by specified parameters
 * @param dev: The audio in device
 * @param param: The audio in parameter setting
 * @return The audio in handle
 */
static inline void* audio_in_open(struct device *dev, ain_param_t *setting)
{
	const struct ain_driver_api *api = dev->driver_api;

	return api->ain_open(dev, setting);
}

/*!
 * @brief Close the audio in channel by specified handle
 * @param dev: The audio in device
 * @param handle: The audio in handle
 * @return 0 on success, negative errno code on fail
 */
static inline int audio_in_close(struct device *dev, void *handle)
{
	const struct ain_driver_api *api = dev->driver_api;

	return api->ain_close(dev, handle);
}

/*!
 * @brief Disable the audio in channel by specified handle
 * @param dev: The audio in device
 * @param handle: The audio in handle
 * @param cmd: The audio in command
 * @param param: The audio in in/out parameters corresponding with the commands
 * @return 0 on success, negative errno code on fail
 */
static inline int audio_in_control(struct device *dev, void *handle, int cmd, void *param)
{
	const struct ain_driver_api *api = dev->driver_api;

	return api->ain_control(dev, handle, cmd, param);
}

/*!
 * @brief Start the audio in channel
 * @param dev: The audio in device
 * @param handle: The audio in handle
 * @return 0 on success, negative errno code on fail
 */
static inline int audio_in_start(struct device *dev, void *handle)
{
	const struct ain_driver_api *api = dev->driver_api;

	return api->ain_start(dev, handle);
}

/*!
 * @brief Stop the audio in channel
 * @param dev: The audio in device
 * @param handle: The audio in handle
 * @return 0 on success, negative errno code on fail
 */
static inline int audio_in_stop(struct device *dev, void *handle)
{
	const struct ain_driver_api *api = dev->driver_api;

	return api->ain_stop(dev, handle);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* __AUDIO_IN_H__ */
