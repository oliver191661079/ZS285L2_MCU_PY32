/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arithmetic.h>
#include <audio_system.h>
#include <audio_policy.h>
#include <media_player.h>
#include <media_effect_param.h>
#include <bluetooth/bt_manager.h>
#include <volume_manager.h>
#include <app_manager.h>
#include "app_defines.h"
#include "tool_app_inner.h"

#ifdef CONFIG_DVFS
#include <dvfs.h>
#endif

#define RETT_STUB_RW_BUF_SIZE	(256)
#define RETT_RECONNECT_TIMES	(10)
typedef struct {
	int update_flag;	//1--need to update param
	int length;
	int reserve;
} stUpdate_info_t;

typedef struct {
	short reverbEnable;	//0 reverb mutex with echo
	short reverbRoomSize;	//75
	short reverbPreDelay;	//10
	short reverbReverberance;	//50
	short reverbHfDamping;	//50
	short reverbToneLow;	//32
	short reverbToneHigh;	//8372
	short reverbWetGain;	//-1
	short reverbDryGain;	//-1
	short reverbStereoWidth;	//100
	short reverbWetOnly;	//0
	short reverDecay;
	short reserve[6];	//0
} reverb_param_t;

typedef struct {
	short echoEnable;	//1 //1 reverb mutex with echo
	short echoDelay;	//200ms
	short echoDecay;	//5
	short echoLowFrq1;	//0～20KHz
	short echoLowFrq2;	//0～20KHz
	short echoVoiceDecay;	//0.0dB ~ -12.0dB，default -6.0dB，unit 0.1dB
	short reserve[2];
} echo_param_t;

typedef struct {
	short changeFlag;	//1                            //1
	short bypass;		//0
	reverb_param_t reverb_para;
	echo_param_t echo_para;
	u8_t szRev[72];

} Karaoke_para_t;

typedef struct {
	u32_t connected_status:1;
	u32_t connect_fail_times:8;
	Karaoke_para_t Karaoke_para;
	char *stub_rw_buf;
} RETT_data_t;

static void _rett_data_free(RETT_data_t * data);

static RETT_data_t *_rett_data_alloc()
{
	RETT_data_t *data = app_mem_malloc(sizeof(*data));
	if (!data)
		return NULL;

	memset(data, 0, sizeof(*data));

	data->stub_rw_buf = app_mem_malloc(RETT_STUB_RW_BUF_SIZE);
	if (!data->stub_rw_buf)
		goto err_exit;

	return data;
 err_exit:
	_rett_data_free(data);
	return NULL;
}

static void _rett_data_free(RETT_data_t * data)
{
	if (data->stub_rw_buf)
		app_mem_free(data->stub_rw_buf);

	app_mem_free(data);
}

static int _rett_stub_read(RETT_data_t * data, uint16_t opcode, void *buf,
			   uint16_t size)
{
	stub_ext_param_t param = {
		.opcode = opcode,
		.payload_len = 0,
		.rw_buffer = data->stub_rw_buf,
	};
	int ret = 0;

	memset(data->stub_rw_buf, 0, RETT_STUB_RW_BUF_SIZE);

	/* plus 2 bytes checksum */
	assert(size + sizeof(stub_ext_cmd_t) + 2 <= RETT_STUB_RW_BUF_SIZE);

	/* send read request */
	ret = stub_ext_write(tool_stub_dev_get(), &param);
	if (ret) {
		SYS_LOG_ERR("read request failed");
		return ret;
	}

	memset(data->stub_rw_buf, 0, RETT_STUB_RW_BUF_SIZE);

	/* receive data */
	param.payload_len = size;
	ret = stub_ext_read(tool_stub_dev_get(), &param);
	if (ret) {
		SYS_LOG_ERR("read data failed %p %d", buf, size);
		return ret;
	}

	memcpy(buf, &param.rw_buffer[sizeof(stub_ext_cmd_t)], size);

	memset(data->stub_rw_buf, 0, RETT_STUB_RW_BUF_SIZE);

	/* send ack */
	param.opcode = STUB_CMD_RETT_ACK;
	param.payload_len = 0;
	ret = stub_ext_write(tool_stub_dev_get(), &param);
	if (ret) {
		SYS_LOG_ERR("write ack failed");
		return ret;
	}

	return 0;
}

static int _rett_set_Karaoke_param(RETT_data_t * data)
{
	int ret =
	    _rett_stub_read(data, STUB_CMD_RETT_READ_KARAOK_PARAM,
			    &data->Karaoke_para, sizeof(Karaoke_para_t));

	if (!ret) {
		if (data->Karaoke_para.echo_para.echoEnable) {
			media_player_okmic_ctx_s *okmic_ctx_s =
			    get_okmic_player_ctx();
			echo_para_t echo_para = { 0 };

			if (!okmic_ctx_s || !okmic_ctx_s->player)
				return -1;

			echo_para.echo_enable =
			    data->Karaoke_para.echo_para.echoEnable;
			echo_para.echo_delay =
			    data->Karaoke_para.echo_para.echoDelay;
			echo_para.echo_decay =
			    data->Karaoke_para.echo_para.echoDecay;
			echo_para.echo_lowpass_fc1 =
			    data->Karaoke_para.echo_para.echoLowFrq1;
			echo_para.echo_lowpass_fc2 =
			    data->Karaoke_para.echo_para.echoLowFrq2;
			echo_para.decay_times =
			    data->Karaoke_para.echo_para.reserve[0];

			media_player_update_okmic_mic_bypass_param(okmic_ctx_s->
								   player,
								   data->
								   Karaoke_para.
								   bypass);
			media_player_set_okmic_mic_vol(okmic_ctx_s->player,
						       data->Karaoke_para.
						       echo_para.
						       echoVoiceDecay);
			media_player_update_okmic_echo_param(okmic_ctx_s->
							     player,
							     &echo_para);

			printk("bypass %d\n", data->Karaoke_para.bypass);
			printk("echoVoiceDecay %d\n",
			       data->Karaoke_para.echo_para.echoVoiceDecay);
			printk("echo_delay %d\n", echo_para.echo_delay);
			printk("echo_decay %d\n", echo_para.echo_decay);
			printk("echo_lowpass_fc1 %d\n",
			       echo_para.echo_lowpass_fc1);
			printk("echo_lowpass_fc2 %d\n",
			       echo_para.echo_lowpass_fc2);
			printk("decay_times %d\n", echo_para.decay_times);
		}

		if (data->Karaoke_para.reverb_para.reverbEnable) {
			media_player_okmic_ctx_s *okmic_ctx_s =
			    get_okmic_player_ctx();
			reverb_para_t reverb_para = { 0 };

			if (!okmic_ctx_s || !okmic_ctx_s->player)
				return -1;

			reverb_para.mRoomSize =
			    data->Karaoke_para.reverb_para.reverbRoomSize;
			reverb_para.mPreDelay =
			    data->Karaoke_para.reverb_para.reverbPreDelay;
			reverb_para.mReverberance =
			    data->Karaoke_para.reverb_para.reverbReverberance;
			reverb_para.mHfDamping =
			    data->Karaoke_para.reverb_para.reverbHfDamping;
			reverb_para.mToneLow =
			    data->Karaoke_para.reverb_para.reverbToneLow;
			reverb_para.mToneHigh =
			    data->Karaoke_para.reverb_para.reverbToneHigh;
			reverb_para.mWetGain =
			    data->Karaoke_para.reverb_para.reverbWetGain;
			reverb_para.mDryGain =
			    data->Karaoke_para.reverb_para.reverbDryGain;
			reverb_para.mStereoWidth =
			    data->Karaoke_para.reverb_para.reverbStereoWidth;
			reverb_para.mWetOnly =
			    data->Karaoke_para.reverb_para.reverbWetOnly;
			media_player_update_okmic_mic_bypass_param(okmic_ctx_s->
								   player,
								   data->
								   Karaoke_para.
								   bypass);
			media_player_set_okmic_mic_vol(okmic_ctx_s->player,
						       data->Karaoke_para.
						       reverb_para.reverDecay);
			media_player_update_okmic_reverb_param(okmic_ctx_s->
							       player,
							       &reverb_para);
		}
	}

	return ret;

}

static int _rett_get_update_info_flag(RETT_data_t * data,
				      stUpdate_info_t * stUpdate_info)
{
	return _rett_stub_read(data, STUB_CMD_RETT_READ_UPDATE_INFO_FLAG,
			       stUpdate_info, sizeof(stUpdate_info_t));

}

static int _rett_cmd_deal(RETT_data_t * data)
{
	stUpdate_info_t stUpdate_info = { 0 };
	if (_rett_get_update_info_flag(data, &stUpdate_info)) {
		if (++data->connect_fail_times >= RETT_RECONNECT_TIMES)
			data->connected_status = 1;
	} else {
		data->connect_fail_times = 0;
		data->connected_status = 0;
	}

	if (stUpdate_info.update_flag)	/*need to update karaoke param */
		_rett_set_Karaoke_param(data);

	return 0;
}

static int _rett_send_ack(RETT_data_t * data)
{
	stub_ext_param_t param = {
		.opcode = STUB_CMD_RETT_ACK,
		.rw_buffer = data->stub_rw_buf,
		.payload_len = 0,
	};
	memset(data->stub_rw_buf, 0, RETT_STUB_RW_BUF_SIZE);
	return stub_ext_write(tool_stub_dev_get(), &param);
}

void tool_rett_loop(void)
{
	RETT_data_t *rett_data = NULL;

	SYS_LOG_INF("Enter");

#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
	dvfs_set_level(DVFS_LEVEL_HIGH_PERFORMANCE, "tool");
#endif

	rett_data = _rett_data_alloc();
	if (!rett_data) {
		SYS_LOG_ERR("data_alloc failed");
		goto out_exit;
	}

	if (_rett_send_ack(rett_data)) {
		SYS_LOG_ERR("send_ack failed");
		goto out_exit;
	}

	do {
		os_sleep(10);

		/* connect status detect */
		if (rett_data->connected_status) {
			SYS_LOG_ERR("disconnected\n");
			break;
		}

		_rett_cmd_deal(rett_data);

	} while (!tool_is_quitting());

 out_exit:
	if (rett_data)
		_rett_data_free(rett_data);

#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
	dvfs_unset_level(DVFS_LEVEL_HIGH_PERFORMANCE, "tool");
#endif

	SYS_LOG_INF("Exit");
}
