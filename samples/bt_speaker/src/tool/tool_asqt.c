/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <audio_system.h>
#include <audio_policy.h>
#include <media_mem.h>
#include <media_player.h>
#include <media_effect_param.h>
#include <app_manager.h>
#include <tts_manager.h>
#include <volume_manager.h>
#include <bt_manager.h>
#include <shell/shell.h>
#include "app_defines.h"
#include "tool_app_inner.h"
#include "app_launch.h"

extern int audio_policy_change_mic_gain_to_db(u16_t analog_gain, u16_t  digital_gain);

#ifdef CONFIG_SOC_DVFS
#include <soc_dvfs.h>
#endif

#if (1 == CONFIG_DSP_HFP_VERSION)
void audio_system_volume_update_voice_table(u8_t *data, u8_t num);
void audio_system_volume_update_voice_dec(u8_t left, u8_t right);
#endif

#define ASQT_PCM_DATA_EXCHANGE
#define ASQT_PARAM_UPDATE

#if (0 == CONFIG_DSP_HFP_VERSION)
#define ASQT_DSP_RESET
#endif

#define ASQT_LAZY_DELAY 1000
#define ASQT_WORK_DELAY 200
#define ASQT_BUSY_DELAY 1
#if (1 == CONFIG_DSP_HFP_VERSION)
#define ASQT_EFFECT_PBUF_SIZE (0x300)
#else
#define ASQT_EFFECT_PBUF_SIZE (81 * sizeof(int))
#endif

#define ASQT_DUMP_BUF_SIZE	(1024)
#define ASQT_DSPBUF_SIZE ASQT_DUMP_BUF_SIZE
#if (1 == CONFIG_DSP_HFP_VERSION)
#define ASQT_MAX_DUMP       (7)
#else
#define ASQT_MAX_DUMP		(6)
#endif

#define ASQT_MAX_DUMP_CURRENT	(4)

#define ASQT_STUB_BUF_SIZE	ROUND_UP(STUB_COMMAND_HEAD_SIZE + 1 + (1024 + 1) * 2, 2)

#define CAPTURE_START_INDEX (2)

static const uint8_t asqt_tag_table[ASQT_MAX_DUMP] = {
	/* playback tags */
	MEDIA_DATA_TAG_DECODE_OUT1,
	MEDIA_DATA_TAG_PLC,
	/* capture tags */
	MEDIA_DATA_TAG_REF,
	MEDIA_DATA_TAG_MIC1,
	MEDIA_DATA_TAG_AEC1,
	MEDIA_DATA_TAG_ENCODE_IN1,
#if (1 == CONFIG_DSP_HFP_VERSION)
	MEDIA_DATA_TAG_DRC,
#endif
};

struct ASQT_data {
	uint8_t has_effect_set:1;
	uint8_t in_simulation:1;

	uint8_t in_btcall:1;
	uint8_t last_in_btcall:1;

	uint8_t run_status:1;
	uint8_t last_run_status:1;

	uint8_t upload_status:1;
	uint8_t last_upload_status:1;

	uint8_t btcall_continue:1;

	uint8_t simulation_started:1;
	uint8_t simulation_pause_get_data:1;
	uint32_t simulation_last_get_data_time;

	uint8_t reset_dsp_flag;
	uint8_t codec_id;

	/* asqt para */
	void *hfp_para;

	/* asqt dump tag select */
	asqt_ST_ModuleSel modesel;
	struct acts_ringbuf *dump_bufs[ARRAY_SIZE(asqt_tag_table)];

	/* backstore memory for dump_bufs */
	char *dump_backstore;
	/* stub rw buffer */
	char *stub_buf;
};

struct ASQT_data *asqt_data = NULL;
#ifdef ASQT_PCM_DATA_EXCHANGE
static int asqt_prepare_data_upload(struct ASQT_data *data);
static void asqt_unprepare_data_upload(struct ASQT_data *data);
#endif
static int asqt_prepare_simulation(struct ASQT_data *data);
static int asqt_unprepare_simulation(struct ASQT_data *data);

#ifdef ASQT_PARAM_UPDATE
static int asqt_config_effect_mem(void *effect_mem)
{
	if (NULL == effect_mem) {
		SYS_LOG_INF("unset user param.");
		media_effect_set_user_param(AUDIO_STREAM_VOICE, 0, NULL, 0);
	} else {
		SYS_LOG_INF("set user param.");
        	media_effect_set_user_param(AUDIO_STREAM_VOICE, 0, effect_mem, ASQT_PARA_REAL_SIZE);
	}
	return 0;
}
#endif

#ifdef ASQT_PCM_DATA_EXCHANGE
#if 0
#include <libbt_engine/bt_eg_config.h>
#else
typedef struct {
	int16_t pkt_status;
	int16_t seq_no;
	int16_t frame_len;
	int16_t padding_len;
} sco_dsp_pkt_t;
#endif


static int asqt_check_data(uint8_t type, uint8_t * data, uint16_t len)
{
	if (type == MSBC_TYPE) {
		if ((data[0] == 0 && data[1] == 0 && data[2] == 0xAD
		     && data[3] == 0x00)) {
		} else {
			SYS_LOG_INF("DATA_FORMAT_IS_WRONG\n");
			return 0;
		}
		if (len == 60) {
		} else {
			SYS_LOG_INF("DATA_LEN_ERROR\n");
			return 0;
		}
	} else if (type == CVSD_TYPE) {
		if ((data[0] == 0 && data[1] == 0)) {
		} else {
			SYS_LOG_INF("DATA_FORMAT_IS_WRONG\n");
			return 0;
		}
		if (len == 62) {
		} else {
			SYS_LOG_INF("DATA_LEN_ERROR\n");
			return 0;
		}
	} else {
		SYS_LOG_INF("format error\n");
		return 0;
	}

	return 1;
}

static int asqt_write_simulation_data(io_stream_t bt_stream, uint8_t * data,
				      uint16_t len)
{
	static u32_t seq_num = 0;
	sco_dsp_pkt_t sco_pkt;
	uint16_t write_len;
	uint8_t *buff;
	int ret;

	sco_pkt.pkt_status = 0;
	sco_pkt.frame_len = len;
	sco_pkt.seq_no = seq_num++;
	sco_pkt.padding_len = (4 - sco_pkt.frame_len % 4) % 4;

	write_len = sizeof(sco_pkt) + sco_pkt.frame_len + sco_pkt.padding_len;

	buff = app_mem_malloc(write_len);
	if (NULL == buff) {
		SYS_LOG_INF("MALLOC_FAILS");
		return 0;
	}

	/* fix cvsd end noise bug */
	if (asqt_data->codec_id == CVSD_TYPE) {
		if (data[0] == 0x0 && memcmp(data, data + 1, len - 1) == 0) {
			memset(data, 0x55, len);
		}
		/* print_buffer(data, 1, 16, 32, 0xF0000000); */
	}
	memcpy(buff, (void *)&sco_pkt, sizeof(sco_pkt));
	memcpy(buff + sizeof(sco_pkt), (void *)data, sco_pkt.frame_len);
	memset(buff + sizeof(sco_pkt) + sco_pkt.frame_len, 0,
	       sco_pkt.padding_len);

	ret = stream_write(bt_stream, buff, write_len);

	app_mem_free(buff);
	return ret;
}
#endif

static struct ASQT_data *asqt_data_alloc(void)
{
	struct ASQT_data *data = app_mem_malloc(sizeof(*data));
	if (!data)
		return NULL;

	memset(data, 0, sizeof(*data));

	if (media_mem_get_cache_pool_size
	    (TOOL_ASQT_STUB_BUF, AUDIO_STREAM_VOICE) < ASQT_STUB_BUF_SIZE) {
		SYS_LOG_ERR("TOOL_ASQT_STUB_BUF too small.\n");
		app_mem_free(data);
		return NULL;
	}



	data->stub_buf =
	    media_mem_get_cache_pool(TOOL_ASQT_STUB_BUF, AUDIO_STREAM_VOICE);
	data->dump_backstore =
	    (char *)app_mem_malloc(ASQT_DUMP_BUF_SIZE * ASQT_MAX_DUMP_CURRENT);
	if (NULL == data->dump_backstore) {
		app_mem_free(data);
		SYS_LOG_ERR("malloc backstore FA");
		return NULL;
	}

	data->hfp_para = app_mem_malloc(ASQT_EFFECT_PBUF_SIZE);

	if (NULL == data->hfp_para) {
		app_mem_free(data->dump_backstore);
		app_mem_free(data);
		SYS_LOG_ERR("malloc hfp FA");
		return NULL;
	}

	return data;
}

static void asqt_data_free(struct ASQT_data *data)
{

	if (NULL == data) {
		return;
	}

	if (NULL != data->hfp_para) {
		app_mem_free(data->hfp_para);
		data->hfp_para = NULL;
	}

	if (NULL != data->dump_backstore) {
		app_mem_free(data->dump_backstore);
		data->dump_backstore = NULL;
	}

	data->stub_buf = NULL;
	app_mem_free(data);
}

static int asqt_stub_read_data(int opcode, void *buf, unsigned int size)
{
	int ret = 0;

	SYS_LOG_DBG("0x%x", opcode);
	ret = stub_get_data(tool_stub_dev_get(), opcode, buf, size);
	if (ret) {
		SYS_LOG_WRN("%d", ret);
	}

	return ret;
}

static int asqt_stub_write_data(int opcode, void *buf, unsigned int size)
{
	int ret = 0;
	int err_cnt = 0;

 retry:
	SYS_LOG_DBG("0x%x", opcode);
	ret = stub_set_data(tool_stub_dev_get(), opcode, buf, size);
	if (ret) {
		SYS_LOG_WRN("%d %d", ret, err_cnt);
		if (++err_cnt < 10)
			goto retry;
	}

	return ret;
}

static int asqt_stub_upload_case_info(struct ASQT_data *data)
{
	asqt_interface_config_t *p_config_info =
	    (asqt_interface_config_t *) (data->stub_buf +
					 STUB_COMMAND_HEAD_SIZE);
	const char *fw_version = CONFIG_TOOL_PROTOCOL_VERSION;

	SYS_LOG_INF("codec type = %d", data->codec_id);

	memset(p_config_info, 0, sizeof(asqt_interface_config_t));
	strncpy((char *)p_config_info->fw_version, fw_version,
		sizeof(p_config_info->fw_version));
	p_config_info->eq_point_nums = 20;
	p_config_info->eq_version = 0;
	p_config_info->sample_rate = (data->codec_id == CVSD_TYPE) ? 0 : 1;

	asqt_stub_write_data(STUB_CMD_ASQT_WRITE_CONFIG_INFO, data->stub_buf,
			     sizeof(asqt_interface_config_t));
	return 0;
}

static int asqt_stub_get_status(void)
{
	int32_t status = -1;
	asqt_stub_read_data(STUB_CMD_ASQT_READ_PC_TOOL_STATUS, &status, 4);
	return status;
}

#ifdef ASQT_PARAM_UPDATE
static int asqt_stub_get_dsp_param_status(void)
{
	int32_t status = -1;
	asqt_stub_read_data(STUB_CMD_ASQT_GET_PC_DSP_PARAM_STATUS, &status, 4);
	return status;
}

static int asqt_stub_dsp_param_deal(struct ASQT_data *data)
{
	int ret;

	SYS_LOG_DBG("");
	ret =
	    asqt_stub_read_data(STUB_CMD_ASQT_GET_DSP_PARAM, data->hfp_para,
				ASQT_EFFECT_PBUF_SIZE);
	if (0 == ret) {
		media_player_t *player;
		player = media_player_get_current_dumpable_player();
		if (!player) {
			SYS_LOG_ERR("no player");
			return -EFAULT;
		}

		media_player_update_effect_param(player, data->hfp_para, ASQT_PARA_REAL_SIZE);

		SYS_LOG_INF("effect updated.");
	}

	return ret;
}

static int asqt_stub_get_case_param_status(void)
{
	int32_t status = -1;
	asqt_stub_read_data(STUB_CMD_ASQT_GET_PC_CASE_PARAM_STATUS, &status, 4);
	return status;
}

static void asqt_update_control_praram(asqt_stControlParam * cp)
{
	int db;
	u16_t a_gain;
	u16_t d_gain;

#if (1 == CONFIG_DSP_HFP_VERSION)
	a_gain = cp->mic0_a_gain;
	d_gain = cp->mic0_d_gain;
	audio_system_volume_update_voice_table((u8_t *) cp->vol_table,
					       cp->volume_levels);
	if (cp->pa_sel == 2) {	//right
		audio_system_volume_update_voice_dec(0,
						     (u8_t) cp->pa_dec_right);
	} else if (cp->pa_sel == 1) {	//left
		audio_system_volume_update_voice_dec((u8_t) cp->pa_dec_left, 0);
	} else {
		audio_system_volume_update_voice_dec((u8_t) cp->pa_dec_left,
						     (u8_t) cp->pa_dec_right);
	}
	system_player_volume_set(AUDIO_STREAM_VOICE, cp->speaker_vol);
	SYS_LOG_INF("vol:%d, pa_sel:%d, levels:%d", cp->speaker_vol, cp->pa_sel, cp->volume_levels);
#else
	a_gain = cp->sAnalogGain;
	d_gain = cp->sDigitalGain;

	audio_system_set_stream_pa_volume(AUDIO_STREAM_VOICE, cp->sMaxVolume * 1000);
	SYS_LOG_INF("pa_db:%d", cp->sMaxVolume);
#endif
	db = audio_policy_change_mic_gain_to_db(a_gain, d_gain);
	SYS_LOG_INF("gain %danalog + %ddigital = %ddB", a_gain, d_gain, db);
	audio_system_set_microphone_volume(AUDIO_STREAM_VOICE, db);
}

static asqt_stControlParam case_param;
static int asqt_stub_case_param_deal(struct ASQT_data *data)
{
	media_player_t *player;
	int ret = 0;

	SYS_LOG_DBG("");
	player = media_player_get_current_dumpable_player();
	if (!player) {
		SYS_LOG_ERR("no player");
		return -EFAULT;
	}

	ret =
	    asqt_stub_read_data(STUB_CMD_ASQT_GET_CASE_PARAM, &case_param,
				sizeof(case_param));
	if (!ret) {
		asqt_update_control_praram(&case_param);
	}

	return ret;
}

static int asqt_update_params(struct ASQT_data *data)
{
	static bool case_update_pending = false;
	static bool dsp_update_pending = false;
	int ret = 0;

	if (case_update_pending || asqt_stub_get_case_param_status() == 1) {
		if(0 == asqt_stub_case_param_deal(data)) {
			ret |= 1;
			case_update_pending = false;
		} else {
			case_update_pending = true;
		}
	}

	if (dsp_update_pending || asqt_stub_get_dsp_param_status() == 1) {
		if(0 == asqt_stub_dsp_param_deal(data)) {
			ret |= 2;
			dsp_update_pending = false;
		} else {
			dsp_update_pending = true;
		}
	}

	return ret;
}
#endif

#ifdef ASQT_PCM_DATA_EXCHANGE
static int asqt_get_modsel(struct ASQT_data *data, asqt_ST_ModuleSel * modsel)
{
	return asqt_stub_read_data(STUB_CMD_ASQT_GET_MODESEL, modsel,
				   sizeof(*modsel));
}

static int asqt_prepare_data_upload(struct ASQT_data *data)
{
	char *dumpbuf = data->dump_backstore;
	int num_tags = 0;

	media_player_t *player = media_player_get_current_dumpable_player();
	if (!player) {
		SYS_LOG_ERR("no player");
		return -EFAULT;
	}

	if (asqt_get_modsel(data, &data->modesel)) {
		SYS_LOG_ERR("get_modsel failed");
		return -EIO;
	}
#if 0
	SYS_LOG_DBG("Forcely select demo");
	data->modesel.bSelArray[0] = 1;
#endif

	for (int i = 0; i < ARRAY_SIZE(asqt_tag_table); i++) {
		if (!data->modesel.bSelArray[i]) {
			SYS_LOG_DBG("%d not selected", i);
			continue;
		} else {
			SYS_LOG_DBG("%d selected",  i);
		}

		if (num_tags++ >= ASQT_MAX_DUMP_CURRENT) {
			SYS_LOG_ERR("exceed max %d tags", ASQT_MAX_DUMP_CURRENT);
			//return -EFAULT;
    		break;
		}

		/* update to data tag */
		data->dump_bufs[i] =
		    acts_ringbuf_init_ext(dumpbuf, ASQT_DUMP_BUF_SIZE);
		if (!data->dump_bufs[i]) {
			SYS_LOG_WRN("no dump buf %d", i);
			asqt_unprepare_data_upload(data);
			return -ENOMEM;
		}

		dumpbuf += ASQT_DUMP_BUF_SIZE;
		SYS_LOG_INF("start idx=%d, tag=%u, buf=%p", i,
			    asqt_tag_table[i], data->dump_bufs[i]);
	}

	return media_player_dump_data(player, ARRAY_SIZE(asqt_tag_table),
				      asqt_tag_table, data->dump_bufs);

}

static void asqt_unprepare_data_upload(struct ASQT_data *data)
{
	for (int i = 0; i < ARRAY_SIZE(data->dump_bufs); i++) {
		if (data->dump_bufs[i]) {
			SYS_LOG_INF("stop idx=%d, tag=%u, buf=%p", i,
				    asqt_tag_table[i], data->dump_bufs[i]);
			media_player_dump_data(NULL, 1, &asqt_tag_table[i],
					       NULL);
			acts_ringbuf_destroy_ext(data->dump_bufs[i]);
			data->dump_bufs[i] = NULL;
		}
	}
}

/* ASQT upload format*/
/* ------------------------------------------------------------------------------
 * | 1 bytes  | 1 bytes |  size   | 1 bytes |  size   | ... | 1 bytes |  size   |
 * ------------------------------------------------------------------------------
 * | num_tags |  tag_0  | data_0  |  tag_1  | data_1  | ... |  tag_n  | data_n  |
 * ------------------------------------------------------------------------------
 * data size is computed by formula:
 * 		size = (payload - 1 / num_tags) - 1
 * in which,
 * 		payload is the total size of the upload package.
 */
static int asqt_upload_data_range(struct ASQT_data *data, int start, int end,
				  int data_size)
{
	uint8_t *data_head = data->stub_buf + STUB_COMMAND_HEAD_SIZE;
	uint8_t *data_buff = data_head;
	uint8_t num_tags = 0, i;

	for (i = start; i < end; i++) {
		if (NULL == data->dump_bufs[i]) {
			SYS_LOG_DBG("no dump buf %d", i);
			continue;
		}

		if (acts_ringbuf_length(data->dump_bufs[i]) < data_size) {
			return 0;
		}

		num_tags++;
	}

	if (num_tags == 0) {
		SYS_LOG_DBG("no point");
		return 0;
	}

	/* fill number of tags */
	*data_buff++ = num_tags;

	for (i = start; i < end; i++) {
		if (!data->dump_bufs[i])
			continue;

		/* fill tag */
		*data_buff++ = i;
		/* fill data */
		acts_ringbuf_get(data->dump_bufs[i], data_buff, data_size);
		data_buff += data_size;
	}

	return asqt_stub_write_data(STUB_CMD_ASQT_UPLOAD_DATA_NEW,
				    data->stub_buf,
				    (unsigned int)(data_buff - data_head));
}

/* cvsd: 120 bytes per decoding output frame;
	*  msbc: 240 bytes per decoding output frame.
*/
#define DATA_UNIT1 (120*2)
#define DATA_UNIT2 (256)

static int asqt_upload_data(struct ASQT_data *data)
{
	int ret = 0;
	int len1 = DATA_UNIT1;
	int len2 = DATA_UNIT2;

	if (data->codec_id == MSBC_TYPE) {
		len1 *= 2;
		len2 *= 2;
	}

	ret = asqt_upload_data_range(data, 0, 1, len1);
	ret = asqt_upload_data_range(data, 1, 2, len2);
	ret = asqt_upload_data_range(data, 2, 3, len2);
	ret = asqt_upload_data_range(data, 3, 4, len2);
	ret = asqt_upload_data_range(data, 4, 5, len2);
	ret = asqt_upload_data_range(data, 5, 6, len1);
	ret = asqt_upload_data_range(data, 6, 7, len2);

	return ret;
}
#endif

static int asqt_get_simulation_mode(struct ASQT_data *data)
{
	int ret = 0;
	int mode;
	uint8_t analysis_mode = 1;

	mode = 1;
	ret =
	    asqt_stub_read_data(STUB_CMD_ASQT_GET_ANALYSIS_MODE, &analysis_mode,
				1);
	if (0 == ret) {
		mode = !analysis_mode;
		SYS_LOG_INF("simulate mode=%d", mode);
	} else {
		SYS_LOG_ERR("Fail to get analysis mode.");
	}

	return mode;
}

int monitor_callback(u16_t pcm_time,u16_t normal_level,u8_t aps_status)
{
	if (asqt_data->in_simulation) {
		// printk("pcm:%d level:%d\n", pcm_time, normal_level);
		if (!asqt_data->simulation_started) {
			if (pcm_time > audio_policy_get_out_input_start_threshold(AUDIO_STREAM_VOICE, asqt_data->codec_id, 0 ,0 ,0 ,0)){
				asqt_data->simulation_started = 1;
			}
		} else {
			if (pcm_time >= normal_level) {
				asqt_data->simulation_pause_get_data = 1;
				return 0;
			}
		}

		asqt_data->simulation_pause_get_data = 0;
	}

	return 0;
}

static int asqt_prepare_simulation(struct ASQT_data *data)
{
	uint8_t hfp_freq = 0;
	int ret = 0;
	uint16_t handle = 0x800;
	uint8_t codec_id;
	struct bt_audio_report audio_rep;

	ret =
	    asqt_stub_read_data(STUB_CMD_ASQT_GET_HFP_FREQ, &hfp_freq,
				sizeof(hfp_freq));
	if (ret) {
		SYS_LOG_ERR("fail to get hfp freq");
		goto err_out;
	}

	SYS_LOG_INF("Add btcall");
	system_app_launch_add(DESKTOP_PLUGIN_ID_BR_CALL);

	data->codec_id = (hfp_freq == 0) ? CVSD_TYPE : MSBC_TYPE;
	audio_asp_set_pcm_monitor_callback(monitor_callback);

	/* FIXME: worakaround for simulation mode performance.
	 *
	 * PC tool requires reporting the codec_id even in simulaton mode, and after reporting,
	 * the PC tool will consider simply the effect param changed agin. So sync params here
	 * in advance to avoid side effect of the poor usb performace on the simulation mode .
	 */
	asqt_stub_upload_case_info(data);
	asqt_update_params(data);

	bt_manager_audio_new_audio_conn_for_test(handle, BT_TYPE_BR,
						 BT_ROLE_SLAVE);

	codec_id =
	    data->codec_id == CVSD_TYPE ? BTSRV_SCO_CVSD : BTSRV_SCO_MSBC;
	bt_manager_sco_set_codec_info(handle, codec_id,
				      (hfp_freq == 0) ? 8 : 16);

	SYS_LOG_INF("enable and start btcall stream");
	audio_rep.handle = handle;
	audio_rep.id = BT_AUDIO_ENDPOINT_CALL;
	audio_rep.audio_contexts = BT_AUDIO_CONTEXT_ASQT_TEST;
	bt_manager_audio_stream_event(BT_AUDIO_STREAM_ENABLE, (void *)&audio_rep,
				      sizeof(struct bt_audio_report));
	bt_manager_audio_stream_event(BT_AUDIO_STREAM_START, (void *)&audio_rep,
				      sizeof(struct bt_audio_report));

	return 0;
 err_out:
	data->in_simulation = 0;
	return ret;
}

static int asqt_unprepare_simulation(struct ASQT_data *data)
{
	uint16_t handle = 0x800;
	struct bt_audio_report audio_rep;

	SYS_LOG_INF("stop&release btcall stream");
	audio_asp_set_pcm_monitor_callback(NULL);
	data->simulation_started = 0;
	data->simulation_last_get_data_time = 0;
	data->simulation_pause_get_data = 0;
	audio_rep.handle = handle;
	audio_rep.id = BT_AUDIO_ENDPOINT_CALL;
	audio_rep.audio_contexts = BT_AUDIO_CONTEXT_CONVERSATIONAL;
	bt_manager_audio_stream_event(BT_AUDIO_STREAM_STOP, (void *)&audio_rep,
				      sizeof(struct bt_audio_report));
	bt_manager_audio_stream_event(BT_AUDIO_STREAM_RELEASE,
				      (void *)&audio_rep,
				      sizeof(struct bt_audio_report));
	bt_manager_audio_delete_audio_conn_for_test(handle);

	//desktop_manager_switch(0, DESKTOP_SWITCH_NEXT);

	return 0;
}



#ifdef ASQT_PCM_DATA_EXCHANGE
static int asqt_run_simulation(struct ASQT_data *data)
{
	uint16_t data_size;
	/* cvsd: 62 bytes per encoding frame; msbc: 60 bytes per encoding frame */


	int min_data_size;
	int data_space;
	io_stream_t bt_stream;
	int ret;



	bt_manager_stream_pool_lock();
	bt_stream = bt_manager_get_stream(STREAM_TYPE_SCO);
	if (NULL == bt_stream) {
		SYS_LOG_WRN("no stream");
		goto out_unlock;
	}
	min_data_size = 68;	//CVSD: 60+8; mSBC: 58+8+2

	data_space = stream_get_space(bt_stream);
	if (data_space < min_data_size) {
		SYS_LOG_DBG("no space");
		goto out_unlock;
	}

	if (data->simulation_pause_get_data) {
		SYS_LOG_DBG("pause read data");
		goto out_unlock;
	}
	if (data->simulation_last_get_data_time && (os_uptime_get_32() - data->simulation_last_get_data_time) < 6){
		SYS_LOG_DBG("filter read data");
		goto out_unlock;
	}

	data_size = (data->codec_id == CVSD_TYPE) ? (62) : (60);
	SYS_LOG_DBG("To download phone, size=%d, space=%d, buf=%d.", data_size,
		    data_space, (int)ASQT_STUB_BUF_SIZE);

	ret =
	    asqt_stub_read_data(STUB_CMD_ASQT_DOWN_REMOTE_PHONE_DATA,
				data->stub_buf, data_size);
	if (ret != 0) {
		SYS_LOG_INF("down remote ret:%d\n", ret);
		goto out_unlock;
	}

	data->simulation_last_get_data_time = os_uptime_get_32();

	if (1 == asqt_check_data(data->codec_id, data->stub_buf, data_size)) {
		ret =
		    asqt_write_simulation_data(bt_stream, data->stub_buf + 2,
					       data_size - 2);
	}

 out_unlock:
	bt_manager_stream_pool_unlock();
	return 0;
}
#endif

static int asqt_btcall_is_on(void)
{
	uint8_t app_id = desktop_manager_get_plugin_id();

	if (app_id != DESKTOP_PLUGIN_ID_BR_CALL) {
		SYS_LOG_DBG("Current is not btcall.\n");
		return 0;
	}


	if (!media_player_get_current_dumpable_player()) {
		SYS_LOG_DBG("no work player.\n");
		return 0;
	}

	return 1;
}

static int asqt_init(void)
{
	SYS_LOG_INF("Enter\n");
#ifdef CONFIG_PLAYTTS
	tts_manager_lock();
#endif

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_set_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "tool");
#endif

	asqt_data = asqt_data_alloc();
	if (!asqt_data) {
		SYS_LOG_ERR("data alloc fail");
		return -1;
	}
	//codec_id = bt_manager_sco_get_codecid();
	asqt_data->codec_id = MSBC_TYPE;

	return 0;

}

static void asqt_exit(void)
{
#ifdef ASQT_PARAM_UPDATE
	/* cancel the effect, since memory will free */
	if (asqt_data->has_effect_set) {
		asqt_config_effect_mem(NULL);
	}
#endif

#ifdef ASQT_PCM_DATA_EXCHANGE
	asqt_unprepare_data_upload(asqt_data);
#endif

	if (asqt_data->in_simulation) {
		asqt_unprepare_simulation(asqt_data);
	}

	if (NULL != asqt_data) {
		asqt_data_free(asqt_data);
		asqt_data = NULL;
	}

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_unset_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "tool");
#endif

#ifdef CONFIG_PLAYTTS
	tts_manager_unlock();
#endif

	SYS_LOG_INF("Exit");
}

static void asqt_prepare_works(void)
{
	//btcall preparing work
	if (asqt_data->in_btcall != asqt_data->last_in_btcall) {
		if (asqt_data->in_btcall) {
			SYS_LOG_DBG("enter btcall");
			asqt_data->in_simulation =
				asqt_get_simulation_mode(asqt_data);
			if (!asqt_data->in_simulation) {
				asqt_data->codec_id =
					(uint8_t)bt_manager_sco_get_codecid();
				SYS_LOG_DBG("Reset codec in btcall, codec=%d",
					    asqt_data->codec_id);
				asqt_stub_upload_case_info(asqt_data);
			}
		} else {
			SYS_LOG_DBG("exit btcall");
		}

		asqt_data->last_in_btcall = asqt_data->in_btcall;
	}

	// Simulation preparing work
	if (asqt_data->run_status != asqt_data->last_run_status) {
		SYS_LOG_DBG("run status %d", asqt_data->run_status);
		asqt_data->in_simulation = asqt_get_simulation_mode(asqt_data);
		if (asqt_data->run_status) {
			if (asqt_data->in_simulation) {
				asqt_prepare_simulation(asqt_data);
			}
		} else {
			if (asqt_data->in_simulation) {
				asqt_unprepare_simulation(asqt_data);
			}
		}

		asqt_data->last_run_status = asqt_data->run_status;
	}

	//Upload preparing work
	if (asqt_data->upload_status != asqt_data->last_upload_status) {
		if (asqt_data->upload_status) {
			SYS_LOG_DBG("upload start");
#ifdef ASQT_PCM_DATA_EXCHANGE
			asqt_prepare_data_upload(asqt_data);
#endif
#ifdef ASQT_PARAM_UPDATE
			asqt_config_effect_mem(asqt_data->hfp_para);
			asqt_data->has_effect_set = 1;
#endif
		} else {
			SYS_LOG_DBG("upload end");
#ifdef ASQT_PCM_DATA_EXCHANGE
			asqt_config_effect_mem(NULL);
			asqt_unprepare_data_upload(asqt_data);
			asqt_stub_write_data(STUB_CMD_ASQT_UPLOAD_DATA_OVER,
					     NULL, 0);
#endif
		}

		asqt_data->last_upload_status = asqt_data->upload_status;
	}
}

void tool_asqt_loop(void)
{
	int status = sNotReady;

	if (asqt_init() != 0) {
		return;
	}

	asqt_stub_upload_case_info(asqt_data);

	while (!tool_is_quitting()) {
		/* get tool status */
		status = asqt_stub_get_status();
		SYS_LOG_INF("status %d", status);
		if (status < 0) {
			os_sleep(ASQT_LAZY_DELAY);
			continue;
		}

		switch (status) {
		case sUserStart:
			asqt_data->run_status = 1;
			break;
		case sUserStop:
		case sReady:
			asqt_data->run_status = 0;
			break;
		case sUserUpdate:
		default:
			break;
		}

#ifdef ASQT_DSP_RESET
		if (asqt_data->reset_dsp_flag > 0) {
			SYS_LOG_DBG("dsp reseting");
			// Warning: Must wait enough time to get new btcall media player.
			os_sleep(ASQT_WORK_DELAY);
			continue;
		}
#endif

		/* get btcall status */
		asqt_data->in_btcall = asqt_btcall_is_on();

#ifdef ASQT_DSP_RESET
		// Do not quit btcall in resetting dsp.
		if (asqt_data->btcall_continue) {
			if (asqt_data->in_btcall) {
				asqt_data->btcall_continue = 0;
#ifdef ASQT_PARAM_UPDATE
				SYS_LOG_INF
				    ("Reload case param after reset btcall.");
				asqt_stub_case_param_deal(asqt_data);
#endif
			} else {
				SYS_LOG_DBG("wait btcall comming");
				os_sleep(ASQT_WORK_DELAY);
				continue;
			}
		}
#endif

		//set upload status
		asqt_data->upload_status =
			(asqt_data->run_status && asqt_data->in_btcall);

		asqt_prepare_works();

		//Download, Upload, and tool setting update.
		if (asqt_data->upload_status) {
			//Need to feed and dumpout stream in time, so could not wait too long time.
#ifdef ASQT_PCM_DATA_EXCHANGE
			for (int i = 0; i < ASQT_WORK_DELAY/ASQT_BUSY_DELAY; i++) {
				if (asqt_data->in_simulation) {
					asqt_run_simulation(asqt_data);
				}
				asqt_upload_data(asqt_data);
				os_sleep(ASQT_BUSY_DELAY);
			}
#else
			os_sleep(ASQT_WORK_DELAY);
#endif
#ifdef ASQT_PARAM_UPDATE
			asqt_update_params(asqt_data);
#endif
		} else {
			SYS_LOG_INF("Not upload.");
			os_sleep(ASQT_LAZY_DELAY);
		}
	}

	asqt_exit();
}

static int shell_dump_hfp(int argc, char *argv[])
{
	SYS_LOG_INF("\n");
	if (NULL != asqt_data) {
		if (NULL != asqt_data->hfp_para) {
			print_buffer_lazy("hfp", asqt_data->hfp_para, ASQT_EFFECT_PBUF_SIZE);
		}
	}
	return 0;
}

static int shell_dump_case_param(int argc, char *argv[])
{
	SYS_LOG_INF("\n");
	if (NULL != asqt_data) {
		print_buffer_lazy("case param", &case_param, sizeof(asqt_stControlParam));
	}
	return 0;
}

static int shell_dump_status(int argc, char *argv[])
{
	if (NULL != asqt_data) {
		for(int i=0; i<ASQT_MAX_DUMP_CURRENT; i++) {
			acts_ringbuf_dump(asqt_data->dump_bufs[i], "D", "\t");
		}
	}
	return 0;
}

const struct shell_cmd asqt_commands[] = {
	{ "dumphfp", shell_dump_hfp, "dump hfp params" },
	{ "dumpcp", shell_dump_case_param, "dump case params" },
	{ "dumpstat", shell_dump_status, "dump status" },
	{ NULL, NULL, NULL }
};

SHELL_REGISTER("asqt", asqt_commands);
