#ifndef __AP_AUTOTEST_CHANNEL_TEST_H
#define __AP_AUTOTEST_CHANNEL_TEST_H

#include <att_pattern_test.h>


#define ADC_SAMPLE_RATE             SAMPLE_8KHZ

#define POWER_LOW_THRESHOLD         (0x1000)

#define MIN_POWER_SAMPLE_COUNT      (4)

#define MAX_POWER_SAMPLE_COUNT      (10)

#define INVALID_POWER_VAL           0xffff

#define MAX_POWER_DIFF_VAL          (0x300)

#define MAX_SAVE_DATA_COUNT         (4)
//#define PRINT_CHANNEL_DATA

#ifdef DEBUG_WRITE_CHANNEL_DATA
extern void write_temp_file(uint8 file_index, uint8 *write_buffer, uint32 write_len);
#endif

extern void adc_data_deal(void);
extern int32 analyse_power_val_valid(uint32 sample_cnt, uint32 *power_val_array,
                uint32 *p_power_val_left, uint32 *p_power_val_right);
extern int32 caculate_power_value(uint8_t *dac_buffer_addr, uint32 data_length, uint32 *power_val_array, uint32 index);


struct att_channel_test_stru{

    audio_out_init_param_t aout_param;
    dac_setting_t dac_setting_param;
    void *aout_handle;
    //dma_setting_t aout_dma_cfg;
    void *ain_handle;

    u8_t*   input_buffer;
	u32_t   input_buffer_size;
	u16_t*  output_buffer;
	u32_t   output_buffer_size;
	void*	snd_in_handle;
	//void*	snd_out_handle;
	uint32  power_val_left;
	uint32  power_val_right;
    uint32  power_val_array[MAX_POWER_SAMPLE_COUNT];
    uint32  sample_count;
    uint8   test_status;
    uint32  count;
    uint32  time_stamp;
    uint32 l_ch_pwr_thr;
    uint32 r_ch_pwr_thr;
    uint8 l_ch_test;
    uint8 r_ch_test;
    struct k_sem channel_test_sem;
};
/**
**	audio out driver data
**/
struct acts_audio_out_data {
	struct device *dma_dev;

	uint8_t pa_open_flag;

	uint8_t aa_channel_open_flag:1;
	uint8_t dac_channel_open_flag:1;
	uint8_t i2s_channel_open_flag:1;
	uint8_t spdif_channel_open_flag:1;
	uint8_t link_dac_i2s_open_flag:1;
	uint8_t link_dac_spdif_open_flag:1;
	uint8_t link_dac_i2s_spdif_open_flag:1;
	uint8_t link_i2s_spdif_open_flag:1;

	uint8_t cur_asrc_out0_ramindex;
	uint8_t cur_asrc_out1_ramindex;

	sys_dlist_t  audio_out_list;
};

extern int audio_out_dma_config(struct device *dev, void *channel_node, dma_setting_t *dma_cfg);

#endif
