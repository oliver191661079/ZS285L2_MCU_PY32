/*
 * Copyright (c) 2020 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief The common audio structures and definitions used within the audio
 */

#ifndef __AUDIO_COMMON_H__
#define __AUDIO_COMMON_H__

/* @brief Definition for FIFO counter */
#define AOUT_FIFO_CNT_MAX               (0xFFFF) /* The max value of the audio out FIFO counter */

/* @brief Definition for max ADC channels */
#define ADC_CH_NUM_MAX                  (2)


/* @brief Definition the invalid type of audio FIFO */
#define AUDIO_FIFO_INVALID_TYPE         (0xFF)

/**
 * @brief Definition for the audio channel type
 * @note If there is a multi-linkage reguirement, it allows using "or" channels.
 *       And it is only allow the linkage usage of the output channels.
 * @par example:
 * (AUDIO_CHANNEL_DAC | AUDIO_CHANNEL_I2STX) stands for DAC linkage with I2S.
 */
#define AUDIO_CHANNEL_DAC               (1 << 0)
#define AUDIO_CHANNEL_I2STX             (1 << 1)
#define AUDIO_CHANNEL_SPDIFTX           (1 << 2)
#define AUDIO_CHANNEL_ADC               (1 << 3)
#define AUDIO_CHANNEL_I2SRX             (1 << 4)
#define AUDIO_CHANNEL_SPDIFRX           (1 << 5)
#define AUDIO_CHANNEL_AA                (1 << 6)
#define AUDIO_CHANNEL_I2SRX1            (1 << 7)

/**
 * @brief Definition for the audio channel status
 */
#define AUDIO_CHANNEL_STATUS_BUSY       (1 << 0) /* channel FIFO still busy status */
#define AUDIO_CHANNEL_STATUS_ERROR      (1 << 1) /* channel error status */

/**
 * @brief Definition for the audio ASRC reset FIFO flag
 */
#define AUDIO_ASRC_RESET_WRITE_FIFO     (1 << 0) /* ASRC reset write FIFO */
#define AUDIO_ASRC_RESET_READ_FIFO      (1 << 1) /* ASRC reset read FIFO */

/**
 * enum audio_sr_sel_e
 * @brief The sample rate choices
 */
typedef enum {
	SAMPLE_RATE_8KHZ   = 8,
	SAMPLE_RATE_11KHZ  = 11,   /* 11.025KHz */
	SAMPLE_RATE_12KHZ  = 12,
	SAMPLE_RATE_16KHZ  = 16,
	SAMPLE_RATE_22KHZ  = 22,   /* 22.05KHz */
	SAMPLE_RATE_24KHZ  = 24,
	SAMPLE_RATE_32KHZ  = 32,
	SAMPLE_RATE_44KHZ  = 44,   /* 44.1KHz */
	SAMPLE_RATE_48KHZ  = 48,
	SAMPLE_RATE_64KHZ  = 64,
	SAMPLE_RATE_88KHZ  = 88,   /* 88.2KHz */
	SAMPLE_RATE_96KHZ  = 96,
	SAMPLE_RATE_176KHZ = 176,  /* 176.4KHz */
	SAMPLE_RATE_192KHZ = 192,
} audio_sr_sel_e;

/*
 * enum audio_aps_level_e
 * @brief The APS for the AUDIO_PLL tuning dynamically
 */
typedef enum {
	APS_LEVEL_1 = 0, /* 44.0083 / 47.8992 */
	APS_LEVEL_2, /* 44.0549 / 47.9508 */
	APS_LEVEL_3, /* 44.0776 / 47.9779 */
	APS_LEVEL_4, /* 44.0857 / 47.9873 */
	APS_LEVEL_5, /* 44.1000 / 48.0000 */
	APS_LEVEL_6, /* 44.1311 / 48.0277 */
	APS_LEVEL_7, /* 44.1532 / 48.0567 */
	APS_LEVEL_8, /* 44.1964 / 48.1045 */
} audio_aps_level_e;

/*
 * enum audio_aps_mode_e
 * @brief The APS mode selection.
 */
typedef enum {
	APS_LEVEL_AUDIOPLL = 0, /* AUDIO_PLL APS tuning */
	APS_LEVEL_ASRC, /* ASRC APS tuning */
	APS_LEVEL_MODE_MAX,
} audio_aps_mode_e;

/*
 * enum audio_ch_mode_e
 * @brief Define the channel mode
 */
typedef enum {
	MONO_MODE = 1,
	STEREO_MODE,
} audio_ch_mode_e;

/*
 * enum audio_outfifo_sel_e
 * @brief Audio out fifo selection
 */
typedef enum {
	AOUT_FIFO_DAC0 = 0, /* DAC FIFO0 */
	AOUT_FIFO_DAC1, /* DAC FIFO1 */
	AOUT_FIFO_I2STX0, /* I2STX FIFO */
} audio_outfifo_sel_e;

/*
 * enum audio_i2s_fmt_e
 * @brief I2S transfer format selection
 */
typedef enum {
	I2S_FORMAT = 0,
	LEFT_JUSTIFIED_FORMAT,
	RIGHT_JUSTIFIED_FORMAT,
	TDM_FORMAT
} audio_i2s_fmt_e;

/*
 * enum audio_i2s_bclk_e
 * @brief Rate of BCLK with LRCLK
 */
typedef enum {
	I2S_BCLK_32BITS = 0,
	I2S_BCLK_16BITS
} audio_i2s_bclk_e;

/*
 * enum audio_i2s_mode_e
 * @brief I2S mode selection
 */
typedef enum {
	I2S_MASTER_MODE = 0,
	I2S_SLAVE_MODE
} audio_i2s_mode_e;

/*
 * enum audio_i2s_srd_wl_e
 * @brief I2S sample rate width length detect
 */
typedef enum {
	SRDSTA_WL_32RATE = 0, /* BCLK = 32LRCLK */
	SRDSTA_WL_64RATE /* BCLK = 64LRCLK */
} audio_i2s_srd_wl_e;

/*
 * enum audio_ch_width_e
 * @brief The effective data width of audio channel
 * @note DAC and SPDIF only support #CHANNEL_WIDTH_16BITS and #CHANNEL_WIDTH_24BITS
 *		I2S support #CHANNEL_WIDTH_16BITS, #CHANNEL_WIDTH_20BITS and #CHANNEL_WIDTH_24BITS
 *		ADC support #CHANNEL_WIDTH_16BITS and #CHANNEL_WIDTH_18BITS
 */
typedef enum {
	CHANNEL_WIDTH_16BITS = 0,
	CHANNEL_WIDTH_18BITS,
	CHANNEL_WIDTH_20BITS,
	CHANNEL_WIDTH_24BITS
} audio_ch_width_e;

/*!
 * enum audio_ext_pa_class_e
 * @brief The external PA class mode selection
 * @note CLASS_AB: higher power consume and lower radiation.
 * ClASS_D: lower power consume and higher radiation.
 */
typedef enum {
	EXT_PA_CLASS_AB = 0,
	EXT_PA_CLASS_D
} audio_ext_pa_class_e;

/*!
 * enum audio_asrc_mode_e
 * @brief The ASRC working mode selection
 */
typedef enum {
	SRC_MODE = 0,
	ASRC_MODE,
	BYPASS_MODE
} audio_asrc_mode_e;

/*!
 * enum audio_asrc_mem_policy_e
 * @brief The policy of selecting the ASRC memory
 */
typedef enum {
    ASRC_ALLOC_AUTO = 0, /* allocate memory automatically */
    ASRC_ALLOC_LOW_LATENCY, /* allocate memory for low latency */
    ASRC_ALLOC_SUBWOOFER, /* allocate memory for subwoofer */
} audio_asrc_mem_policy_e;

/*!
 * enum audio_asrc_out_rclk_e
 * @brief Audio ASRC OUT read clock selection
 */
typedef enum {
	ASRC_OUT_RCLK_CPU = 0,
	ASRC_OUT_RCLK_IIS,
	ASRC_OUT_RCLK_DAC,
} audio_asrc_out_rclk_e;

/*!
 * enum audio_asrc_out_wclk_e
 * @brief Audio ASRC OUT write clock selection
 */
typedef enum {
	ASRC_OUT_WCLK_CPU = 0,
	ASRC_OUT_WCLK_DMA,
	ASRC_OUT_WCLK_DSP,
} audio_asrc_out_wclk_e;

/*!
 * enum audio_asrc_in_rclk_e
 * @brief Audio ASRC IN write clock selection
 */
typedef enum {
	ASRC_IN_RCLK_CPU = 0,
	ASRC_IN_RCLK_DMA,
	ASRC_IN_RCLK_DSP,
} audio_asrc_in_rclk_e;

/*!
 * enum audio_asrc_out_wclk_e
 * @brief Audio ASRC OUT write clock selection
 */
typedef enum {
	ASRC_IN_WCLK_CPU = 0,
	ASRC_IN_WCLK_IISRX1,
	ASRC_IN_WCLK_IISRX0,
} audio_asrc_in_wclk_e;

/*!
 * enum audio_interleave_mode_e
 * @brief Audio DMA interleave mode setting
 */
typedef enum {
	LEFT_MONO_RIGHT_MUTE_MODE = 0,
	LEFT_MUTE_RIGHT_MONO_MODE,
	LEFT_RIGHT_SEPERATE
} audio_interleave_mode_e;

/*!
 * struct audio_spdif_ch_status_t
 * @brief The SPDIF channel status setting
 */
typedef struct {
	uint32_t csl; /* The low 32bits of channel status */
	uint16_t csh; /* The high 16bits of channel status */
} audio_spdif_ch_status_t;

/*!
 * struct audio_reload_t
 * @brief Audio out reload mode configuration.
 * @note If enable reload function the audio driver will transfer the same buffer address and notify the user when the buffer is half full or full.
 */
typedef struct {
	uint8_t *reload_addr; /*!< Reload buffer address to transfer */
	uint32_t reload_len;  /*!< The length of the reload buffer */
} audio_reload_t;

/*!
 * enum ain_source_type_e
 * @brief Audio input devices for low level physical ADC driver.
 */
typedef enum {
	AIN_SOURCE_NONE = 0,
	AIN_SOURCE_AUX0,
	AIN_SOURCE_AUX1,
	AIN_SOURCE_AUX2,
	AIN_SOURCE_AUXFD,
	AIN_SOURCE_ASEMIC,
	AIN_SOURCE_AFDMIC,
    AIN_SOURCE_AFDMIC_AUXFD,
	AIN_SOURCE_ASEMIC_AUXFD,
	AIN_SOURCE_AFDMIC_AUX2,
	AIN_SOURCE_ASEMIC_AUX2,
	AIN_SOURCE_DMIC,
	AIN_SOURCE_I2S0,
	AIN_SOURCE_I2S1,
	AIN_SOURCE_SPDIF,
	AIN_SOURCE_MAX,
} ain_source_type_e;

/*!
 * enum ain_track_e
 * @brief Audio input track channel which is enabled.
 */
typedef enum {
    INPUTSRC_L_R = 0,
    INPUTSRC_ONLY_L,
    INPUTSRC_ONLY_R
} ain_track_e;

/*!
 * struct asrc_sample_rate_t
 * @brief The ASRC sample rate setting for resampling.
 */
typedef struct {
	uint8_t input_sample_rate;
	uint8_t output_sample_rate;
} asrc_sample_rate_t;

/*!
 * struct audio_input_map_t
 * @brief The mapping relationship between audio logic devices and physical devices.
 */
typedef struct {
	uint8_t logical_dev; /* audio logical device */
	uint8_t physical_dev; /* audio physical device */
	uint8_t track_flag; /* flag of audio track channel enabled */
} audio_input_map_t;

#endif /* __AUDIO_COMMON_H__ */
