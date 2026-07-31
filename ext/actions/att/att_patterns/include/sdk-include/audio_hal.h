#ifndef __AUDIO_HAL_H
#define __AUDIO_HAL_H

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
}audio_mode_e;

/*!
 * enum ain_logic_source_type_e
 * @brief Audio input logic devices for middleware and application.
 */
typedef enum {
    AIN_LOGIC_SOURCE_LINEIN = 0,
    AIN_LOGIC_SOURCE_ATT_AUXFD,
    AIN_LOGIC_SOURCE_ATT_AUX0,
    AIN_LOGIC_SOURCE_ATT_AUX1,
    AIN_LOGIC_SOURCE_MIC0,
    AIN_LOGIC_SOURCE_MIC1,
    AIN_LOGIC_SOURCE_FM,
    AIN_LOGIC_SOURCE_DMIC,
    AIN_LOGIC_SOURCE_USB,
} ain_logic_source_type_e;




#define AUDIO_CHANNEL_DAC               (1 << 0)
#define AUDIO_CHANNEL_I2STX             (1 << 1)
#define AUDIO_CHANNEL_SPDIFTX           (1 << 2)
#define AUDIO_CHANNEL_ADC               (1 << 3)
#define AUDIO_CHANNEL_I2SRX             (1 << 4)
#define AUDIO_CHANNEL_SPDIFRX           (1 << 5)


/*
 * enum audio_outfifo_sel_e
 * @brief Audio out fifo selection
 */
typedef enum {
	AOUT_FIFO_DAC0 = 0, /* DAC FIFO0 */
	AOUT_FIFO_DAC1, /* DAC FIFO1 */
	AOUT_FIFO_I2STX0, /* I2STX FIFO */
} audio_outfifo_sel_e;


/**
**	ain track
**/
typedef enum {
    INPUTSRC_L_R = 0,
    INPUTSRC_ONLY_L,
    INPUTSRC_ONLY_R
} ain_track_e;

enum dma_irq_reason {
	DMA_IRQ_TC = 0x0,
	DMA_IRQ_HF,
};


/**
**	input setting Param
**/
typedef struct {
	uint16_t input_gain;
	uint16_t boost;
	ain_track_e ain_track;
} input_setting_t;

/**
**	ADC Setting Param
**/
typedef struct {
	uint8_t adc_sample_rate;
	uint32_t adc_gain;
} adc_setting_t;

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
**	I2S RX Setting Param
**/
typedef struct {
	uint8_t i2srx_sample_rate;
	i2s_device_role_e i2srx_role;
	i2s_fmt_type_e i2srx_format;
} i2srx_setting_t;

/**
**	SPDIF RX Setting Param
**/
typedef struct {
	void (*spdifrx_callback)(void * callback_data, uint32_t param);
	void *callback_data;
} spdifrx_setting_t;

/**
**	asrc logic layer parameters
**/
typedef struct {
	uint32_t ctl_mode; /* 0: src mode;    1: asrc mode */
	uint32_t input_sr;
	uint32_t output_sr;
	uint32_t by_pass;
	uint32_t data_width;
} Logic_asrc_setting_t;

/**
**	usound logic layer parameters
**/
typedef struct {
	void *unused;
} Logic_usound_setting_t;


/**
**  audio in channel logic layer parameters
**/
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
**	PA Setting Param
**/
typedef struct {
	uint8_t left_mute_flag:1;          /* 0: not mute;    1:  mute */
	uint8_t right_mute_flag:1;         /* 0: not mute;    1:  mute */
	uint8_t pa_reserved0:6;
	ain_logic_source_type_e aa_mode_logic_src;     /* 0 for non a-a mode */
	aa_source_e aa_mode_src;           /* Physic source in a-a mdoe, Filled by aa_mode_logic_src */
} pa_setting_t;


typedef struct {
	uint32_t ctl_mode;                          /* 0: src mode;    1: asrc mode */
	uint32_t input_sr;
	uint32_t output_sr;
	uint32_t by_pass;
	uint32_t data_width;
} asrc_setting_t;


/**
**	DAC Setting Param
**/
typedef struct {
	uint8_t dac_sample_rate;					/* DAC  sample rate*/
	uint8_t sample_cnt_enable;					/* fifo sample count */
	uint32_t dac_gain;                          /* DAC gain param, byte 0-da volume; byte 1-pa volume */
} dac_setting_t;


/**
**	I2S TX Setting Param
**/
typedef struct {
	uint8_t i2stx_sample_rate;					/* I2S TX Samplerate KHz */
	i2s_device_role_e i2stx_role;               /* I2STX device rool*/
	i2s_fmt_type_e i2stx_format;				/* I2STX format */
} i2stx_setting_t;

/**
**	SPDIF TX Setting Param
**/
typedef struct {
	uint8_t spdiftx_sample_rate;				/* spdiftx samplerate*/
} spdiftx_setting_t;

/**
**  audio out channel logic layer parameters
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

typedef void (*dma_stream_handle_t)(struct device *dev, u32_t priv_data, int reson);

/**
**  audio dma Setting Param
**/
typedef struct {
	uint8_t *dma_src_buf;
	uint8_t *dma_dst_buf;
	uint32_t dma_trans_len;
	uint8_t dma_width;
	uint8_t dma_reload;
	uint8_t dma_interleaved;
	uint8_t dma_burst_len;
	uint8_t dma_sam;                            /* source address costant */
	dma_stream_handle_t stream_handle;
	void *stream_pdata;
	uint8_t all_complete_callback_en;
	uint8_t half_complete_callback_en;
} dma_setting_t;

extern void* hal_aout_channel_open(audio_out_init_param_t *aout_setting);
extern int hal_aout_channel_close(void *aout_channel_handle);
extern int hal_aout_channel_write_data(void *aout_channel_handle, uint8_t *buff, uint32_t len);
extern int hal_aout_channel_start(void *aout_channel_handle);
extern int hal_aout_channel_stop(void *aout_channel_handle);

extern void* hal_ain_channel_open(audio_in_init_param_t *ain_setting);
extern int32_t hal_ain_channel_close(void *ain_channel_handle);
extern int hal_ain_channel_read_data(void *ain_channel_handle, uint8_t *buff, uint32_t len);
extern int hal_ain_channel_start(void* ain_channel_handle);
extern int hal_ain_channel_stop(void* ain_channel_handle);

#endif

