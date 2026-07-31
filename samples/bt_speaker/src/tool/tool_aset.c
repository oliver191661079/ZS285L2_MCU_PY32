/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arithmetic.h>
#include <audio_system.h>
#include <audio_policy.h>
#include <media_player.h>
#include <audio_policy.h>
#include <sdfs.h>
#include <media_effect_param.h>
#include <bt_manager.h>
#include <volume_manager.h>
#include <app_manager.h>
#include "app_defines.h"
#include "tool_app_inner.h"

#ifdef CONFIG_SOC_DVFS
#include <soc_dvfs.h>
#endif
#include "tool_app_inner.h"

extern void audio_system_volume_update_music_da(u32_t da, u8_t vol);
extern void audio_system_volume_update_music_pa(u8_t pa, u8_t vol);
extern void audio_system_volume_reset_music_pada(void);
void audio_system_volume_update_music_da_dot2ch(const s16_t* da0, const s16_t* da1, u8_t num);

#define ASET_DATA_BUFFER_LEN 2048
#define ASET_SLEEP_TIME 500	//ms
//#define DAE_PARAM_SIZE   (1024)
#define ASET_PARA_BUF_SIZE  ASET_PARA_BIN_SIZE

#ifdef ATS283F_SDK
extern void
bt_manager_register_tws_sync_vol_callback(void (*callback)
					  (uint32_t sync_type,
					   uint32_t media_type, uint32_t vol));
extern void bt_manager_avrcp_sync_vol_by_key(void *dev_info);
#endif

#ifdef CONFIG_TOOL_ASET_PRINT_DEBUG
void audio_effect_music_param_print(void *dae_addr);
#endif

static u8_t *p_aset_rw_buffer = NULL;
static u8_t *p_aset_debug_buffer = NULL;
static volume_tabel_t *p_volume_table = NULL;
static u8_t ch_mode = MULTI_CH_MODE_2_0;

s32_t aset_read_data(u16_t cmd, void *data_buffer, u32_t data_len)
{
	s32_t ret_val;
	stub_ext_param_t ext_param;

	ext_param.opcode = cmd;
	ext_param.payload_len = 0;
	ext_param.rw_buffer = p_aset_rw_buffer;

	if (data_len > ASET_DATA_BUFFER_LEN - 6) {
		return -1;
	}

	SYS_LOG_INF("cmd=0x%x, len=%d", cmd, data_len);
	ret_val = stub_ext_write(tool_stub_dev_get(), &ext_param);

	if (!ret_val) {
		ext_param.payload_len = (u16_t) data_len;

		ret_val = stub_ext_read(tool_stub_dev_get(), &ext_param);
		if (ret_val) {
			SYS_LOG_INF("ret=0x%x", (data_len << 16) + cmd);
		} else {
			memcpy((u8_t *) data_buffer, &(ext_param.rw_buffer[6]),
			       data_len);
			ext_param.opcode = STUB_CMD_ASET_ACK;
			ext_param.payload_len = 0;
			ret_val =
			    stub_ext_write(tool_stub_dev_get(), &ext_param);
		}
	}

	return ret_val;
}

s32_t aset_write_data(u16_t cmd, void *data_buffer, u32_t data_len)
{
	s32_t ret_val;
	s32_t timeout;
	stub_ext_param_t ext_param;

 try_again:
	timeout = 0;
	ext_param.opcode = cmd;
	ext_param.payload_len = data_len;
	ext_param.rw_buffer = p_aset_rw_buffer;

	if (data_len > ASET_DATA_BUFFER_LEN - 6) {
		return -1;
	}

	memcpy(&(ext_param.rw_buffer[6]), data_buffer, data_len);

	SYS_LOG_INF("Write aset command = 0x%x", cmd);
	ret_val = stub_ext_write(tool_stub_dev_get(), &ext_param);
	if (!ret_val) {
		while (1) {
			ext_param.payload_len = 0;
			SYS_LOG_INF("Write aset get ack.");
			ret_val =
			    stub_ext_read(tool_stub_dev_get(), &ext_param);
			if (!ret_val) {
				if ((ext_param.rw_buffer[1] == 0x03)
				    && (ext_param.rw_buffer[2] == 0xfe)) {
					SYS_LOG_INF("Write aset got ack.");
					break;
				}
			} else {
				k_busy_wait(10);
				timeout++;
				SYS_LOG_INF("timeout++");
				if (timeout == 10) {
					//If it fails to receive ACK, re-sends data.
					SYS_LOG_INF("timeout, try again.");
					goto try_again;
				}
			}
		}
	}
	return 0;
}

static void write_application_properties(uint8_t app_id)
{
	int ret_val = 0;

	application_properties_t application_properties;

	SYS_LOG_INF("current ap: %d\n", app_id);

	memset(&application_properties, 0x00, sizeof(application_properties));
	if (app_id != DESKTOP_PLUGIN_ID_LINE_IN) {

		application_properties.aux_mode = UNAUX_MODE;

	} else {

		application_properties.aux_mode = AUX_MODE;

	}

	ret_val = aset_write_data(STUB_CMD_ASET_WRITE_APPLICATION_PROPERTIES,
				  &application_properties,
				  sizeof(application_properties_t));
	if (ret_val) {
		SYS_LOG_INF("write ret=%d", ret_val);
	} else {
		SYS_LOG_INF("aux=%d", application_properties.aux_mode);
	}
}

static void config_pc_view(aset_interface_config_t * pCfg)
{
	const char *fw_version = CONFIG_TOOL_PROTOCOL_VERSION;

	memset(pCfg, 0, sizeof(aset_interface_config_t));

#ifdef CONFIG_AUDIO_MULTI_CHANNEL_MODE
	pCfg->bNR_v_1_0 = 1;
	pCfg->bEQ_v_1_6 = 1;
	pCfg->bVBass_v_5_0 = 1;
	pCfg->bDEQ_v_3_0 = 1;
	if (ch_mode != MULTI_CH_MODE_2_0) {
		pCfg->bSW_v_4_0 = 1;
	}
	pCfg->bMDRC_v_4_1 = 1;
	pCfg->bUD_v_2_0 = 1;
	pCfg->bVolumeTable_v_3_0 = 1;
#else
	//Common config
	pCfg->bMS_v_1_0 = 1;
	pCfg->bDEQ_v_2_0 = 1;
	pCfg->bNR_v_1_0 = 1;

	pCfg->bEQ_v_1_6 = 1;
	pCfg->bUD_v_2_0 = 1;
	pCfg->bVolumeTable_v_3_0 = 1;
	pCfg->bWF_v_1_0 = 1;
	pCfg->bVBass_v_4_0 = 1;

	pCfg->bTE_v_1_1 = 1;
	pCfg->bSurround_v_3_0 = 1;
	pCfg->bComPressor_v_2_0 = 1;
	pCfg->bMDRC_v_3_1 = 1;
#endif
	strncpy(pCfg->szVerInfo, fw_version, sizeof(pCfg->szVerInfo));
}

static s32_t aset_write_case_info(uint8_t app_id)
{
	aset_case_info_t *aset_case_info = malloc(sizeof(aset_case_info_t));

	if (aset_case_info == NULL) {
		return -1;
	}
	memset(aset_case_info, 0x00, sizeof(aset_case_info_t));
	aset_case_info->peq_point_num = EQMAXPOINTCOUNT_ALL;
	aset_case_info->b_Max_Volumel = MAX_AUDIO_VOL_LEVEL;
	aset_case_info->bMultiChannelMode = ch_mode;

	config_pc_view(&aset_case_info->stInterface);

	if (app_id != DESKTOP_PLUGIN_ID_LINE_IN) {

		aset_case_info->aux_mode = UNAUX_MODE;

	} else {

		aset_case_info->aux_mode = AUX_MODE;

	}

	aset_write_data(STUB_CMD_ASET_WRITE_STATUS, aset_case_info,
			sizeof(aset_case_info_t));

	free(aset_case_info);

	return 0;
}

static s32_t aset_volume_deal(uint8_t app_id)
{
	int ret;
	int volume;


	ret = aset_read_data(STUB_CMD_ASET_READ_VOLUME, &volume, sizeof(volume));

	if (!ret) {
		SYS_LOG_INF("vol=%d", volume);
		int stream = AUDIO_STREAM_DEFAULT;
		bool sync = false;
		int sync_vol;
		if (app_id == DESKTOP_PLUGIN_ID_BR_MUSIC){
			stream = AUDIO_STREAM_MUSIC;
			sync = true;
			sync_vol = volume;
		} else if (app_id == DESKTOP_PLUGIN_ID_LINE_IN){
			stream = AUDIO_STREAM_LINEIN;
		} else if (app_id == DESKTOP_PLUGIN_ID_LE_MUSIC) {
			stream = AUDIO_STREAM_LE_AUDIO;
			sync = true;
			sync_vol = volume*255/audio_policy_get_volume_level();
		}

		system_volume_set(stream, volume, false);

		if (sync) {
			bt_manager_volume_client_set(sync_vol);
		}
	}

	return 0;
}

static int aset_get_volume_table_status(void)
{
	int32_t status = -1;
	aset_read_data(STUB_CMD_ASET_READ_VOLUME_TABLE_STATUS, &status, 4);
	return status;
}

static int aset_volume_table_deal(uint8_t app_id)
{
	s32_t ret;

	if ((ch_mode == MULTI_CH_MODE_2_0) || (ch_mode == MULTI_CH_MODE_2_1) ||
	    (ch_mode == MULTI_CH_MODE_2_2) || (ch_mode == MULTI_CH_MODE_2_11)) {
		ret = aset_read_data(STUB_CMD_ASET_READ_VOLUME_TABLE_DATA,
				     p_volume_table, sizeof(volume_tabel_t));
		if (0 == ret) {
			if (p_volume_table->bEnVolumeTabel) {
				SYS_LOG_INF("update da and pa.\n");
				for (int i = 0; i < MAX_AUDIO_VOL_LEVEL + 1;
				     i++) {
					audio_system_volume_update_music_da(p_volume_table->stVolumeInfo[i].sDAVal, i);
					audio_system_volume_update_music_pa((unsigned char)p_volume_table->stVolumeInfo[i].sPAVal, i);
				}

				if ((ch_mode == MULTI_CH_MODE_2_1) || (ch_mode == MULTI_CH_MODE_2_2) || (ch_mode == MULTI_CH_MODE_2_11)) {
					volume_tabel_dot2ch_t *p_vt0 = NULL;
					p_vt0 = malloc(sizeof(volume_tabel_dot2ch_t));
					if (p_vt0 == NULL) {
						SYS_LOG_INF("Failed to malloc.");
						return -1;
					}
					memset(p_vt0, 0, sizeof(volume_tabel_dot2ch_t));

					ret = aset_read_data(STUB_CMD_ASET_READ_VOLUME_TABLE_DATA_DOT2CH, p_vt0, sizeof(volume_tabel_dot2ch_t));
					if (0 == ret) {
						SYS_LOG_INF("update .2ch da.\n");
						audio_system_volume_update_music_da_dot2ch(p_vt0->da0, p_vt0->da1, MAX_AUDIO_VOL_LEVEL + 1);
					} else {
						SYS_LOG_WRN("read data error.");
					}
					free(p_vt0);
				}
			} else {
				SYS_LOG_INF("Reset da and pa.\n");
				audio_system_volume_reset_music_pada();
			}
		} else {
			SYS_LOG_WRN("read data error.");
		}
	}

	if (ch_mode == MULTI_CH_MODE_11) {
		VOLUME_TABLE_011 *p_al = malloc(sizeof(VOLUME_TABLE_011));
		if (p_al == NULL) {
			SYS_LOG_INF("Failed to malloc vt.");
			return -1;
		}
		ret = aset_read_data(STUB_CMD_ASET_READ_VOLUME_TABLE_0_11, p_al, sizeof(VOLUME_TABLE_011));
		if (0 == ret) {
			if (p_al->enable) {
				SYS_LOG_INF("update eM0_1_1.\n");
				s16_t da0[MAX_AUDIO_VOL_LEVEL + 1];
				s16_t da1[MAX_AUDIO_VOL_LEVEL + 1];
				PADA_0_11 *p_a = (PADA_0_11 *)p_al->table;
				for (int i = 0; i <= MAX_AUDIO_VOL_LEVEL; i++) {
					da0[i] = p_a->da0;
					da1[i] = p_a->da1;
					audio_system_volume_update_music_pa(p_a->pa0, i);
					printk("level:%d pa0:%d, da0:%d da1:%d\n", i, p_a->pa0, da0[i], da1[i]);
					p_a++;
				}
				audio_system_volume_update_music_da_dot2ch(da0, da1, MAX_AUDIO_VOL_LEVEL + 1);
			} else {
				SYS_LOG_INF("Reset eM0_1_1\n");
				audio_system_volume_reset_music_pada();
			}
		} else {
			SYS_LOG_WRN("read data error.");
		}
		free(p_al);
	}

	aset_volume_deal(app_id);

	return 0;
}

static s32_t aset_main_switch_deal(void)
{
	int ret = 0;
	int main_switch = 0;
	media_player_t *dump_player =
	    media_player_get_current_dumpable_player();

	if (dump_player) {

		ret =
		    aset_read_data(STUB_CMD_ASET_READ_MAIN_SWITCH, &main_switch,
				   sizeof(main_switch));
		SYS_LOG_INF("main switch=%d", main_switch);
		if (ret == 0) {
			media_player_set_effect_enable(dump_player,
						       main_switch ? true : false);
		}
	} else {
		ret = -1;
	}

	return ret;
}

static s32_t aset_dae_deal(void)
{
	int ret_val = 0;

	media_player_t *dump_player =
	    media_player_get_current_dumpable_player();

	if (!dump_player) {
		return -1;
	}

	ret_val =
	    aset_read_data(STUB_CMD_ASET_READ_DAE_PARAM, p_aset_debug_buffer,
			   ASET_PARA_BUF_SIZE);
	if (!ret_val) {
#ifdef CONFIG_TOOL_ASET_PRINT_DEBUG
		audio_effect_music_param_print(p_aset_debug_buffer);
#endif
		media_player_update_effect_param(dump_player,
						 p_aset_debug_buffer, ASET_PARA_BUF_SIZE);
	}

	//aset_write_data(STUB_CMD_ASET_WRITE_DAE_PARAM, p_aset_debug_buffer, param_size);
	return ret_val;
}

static s32_t aset_upload_file(void)
{
	int ret;
	char *name;
	ASET_FILES *file;
	void *data;
	int len;
	u8_t *buff;
	u8_t endfile[6] = { 0xFF, 0, 0, 0, 0, 0 };

	ASET_FILES files[7] = { {eExf_File,.aux = 1, "aux_16.efx"},
	{eExf_File,.aux = 0, "music_16.efx"},
	{eMusic_File,.mt = eMusic_Pa, "music_pa.bin"},
	{eMusic_File,.mt = eMusic_Da, "music_da.bin"},
	{eMusic_File,.mt = eMusic_Eda, "musiceda.bin"},
	{eMusic_File,.mt = eMusic_1_1, "music1_1.bin"},
	{eNo_File,.reserved = 0, NULL}
	};

	file = &files[0];
	name = file->name;
	while (name != NULL) {
		SYS_LOG_INF("%s", name);
		ret = sd_fmap(name, &data, &len);
		if (ret != 0) {
			SYS_LOG_WRN("Can not load file %s\n", name);
		}

		buff = (u8_t *) malloc(len + 6);
		if (buff == NULL) {
			SYS_LOG_ERR("Not enough memory.");
			break;
		}
		buff[0] = file->t;
		if (file->t == eExf_File) {
			buff[1] = file->aux;
			buff[2] = 16;
			buff[3] = 0;
			buff[4] = len & 0xFF;
			buff[5] = (len >> 8) & 0xFF;
		} else if (file->t == eMusic_File) {
			buff[1] = file->mt;
			buff[2] = 0;
			buff[3] = 0;
			buff[4] = len & 0xFF;
			buff[5] = (len >> 8) & 0xFF;
		} else {
			SYS_LOG_ERR("Wrong file type.");
			free(buff);
			break;
		}
		memcpy(buff + 6, data, len);
		aset_write_data(STUB_CMD_ASET_UPLOAD_FILES, buff, len + 6);
		free(buff);

		file++;
		name = file->name;
	}

	SYS_LOG_INF("End of file.");
	aset_write_data(STUB_CMD_ASET_UPLOAD_FILES, endfile, 6);

	return 0;
}

s32_t aset_cmd_deal(aset_status_t * aset_status, uint8_t app_id)
{
	if (aset_status->upload_case_info == TRUE) {
		aset_write_case_info(app_id);
	}

	if (aset_status->state == 1) {

		if (aset_status->volume_changed == TRUE) {
			SYS_LOG_INF("vol changed.");
			aset_volume_deal(app_id);
		}

		if (aset_status->main_switch_changed == TRUE) {
			SYS_LOG_INF("Mian switch changed.");
			aset_main_switch_deal();
		}

		if (aset_status->dae_changed == TRUE) {
			SYS_LOG_INF("Dae changed.");
			aset_dae_deal();
		}

		if (aset_status->upload_file == TRUE) {
			SYS_LOG_INF("upload file.");
			aset_upload_file();
		}
	}

	return 0;
}

s32_t aset_set_dae_init(uint8_t app_id)
{
	aset_status_t aset_status;

	memset(&aset_status, 0, sizeof(aset_status_t));
	aset_status.state = 1;
	aset_status.volume_changed = 1;
	aset_status.dae_changed = 1;

	aset_write_case_info(app_id);
	write_application_properties(app_id);

	aset_cmd_deal(&aset_status, app_id);
	aset_main_switch_deal();

	return 0;
}

static int _aset_send_ack(void)
{
	stub_ext_param_t param = {
		.opcode = STUB_CMD_ASET_ACK,
		.rw_buffer = p_aset_rw_buffer,
		.payload_len = 0,
	};

	return stub_ext_write(tool_stub_dev_get(), &param);
}

void tool_aset_loop(void)
{
	aset_status_t aset_status;
	media_player_t *dump_player;
	uint8_t app_id;
	uint8_t last_app_id;
	bool app_changed;
	int ret_val = 0;
	u8_t err_cnt;
	bool ae_addr_configed = false;

	err_cnt = 0;
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_set_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "tool");
#endif

	ch_mode = audio_system_get_multi_channel_mode();

	SYS_LOG_INF("malloc");
	p_aset_rw_buffer = malloc(ASET_DATA_BUFFER_LEN);
	if (p_aset_rw_buffer == NULL) {
		SYS_LOG_WRN("malloc 1 failed");
		goto exit;
	}

	p_aset_debug_buffer = malloc(ASET_PARA_BUF_SIZE);
	if (p_aset_debug_buffer == NULL) {
		SYS_LOG_WRN("malloc 2 failed");
		goto exit;
	}
	memset(p_aset_debug_buffer, 0, ASET_PARA_BUF_SIZE);

	p_volume_table = malloc(sizeof(volume_tabel_t));
	if (p_volume_table == NULL) {
		SYS_LOG_WRN("malloc 3 failed");
		goto exit;
	}
	memset(p_volume_table, 0, sizeof(volume_tabel_t));

#ifdef ATS283F_SDK
	//Disable volume synchnization from Phone.
	bt_manager_register_tws_sync_vol_callback(NULL);
#endif
	if (_aset_send_ack()) {
		SYS_LOG_ERR("send_ack failed");
		goto exit;
	}

	last_app_id = desktop_manager_get_plugin_id();

	while (1) {
		//check if app is changed.
		app_id = desktop_manager_get_plugin_id();

		if (app_id != last_app_id) {
			app_changed = true;
		} else {
			app_changed = false;
		}

		last_app_id = app_id;

		if (app_changed) {
			SYS_LOG_INF("App changed.");
			aset_set_dae_init(app_id);
			ae_addr_configed = false;
		}

		// It can only get dumpable player when there is audio stream in processing.
		dump_player = media_player_get_current_dumpable_player();
		if (!dump_player || (app_id == DESKTOP_PLUGIN_ID_BR_CALL)){
			ae_addr_configed = false;

			// Read status to keep aset connecting.
			aset_read_data(STUB_CMD_ASET_READ_STATUS, &aset_status,
				       sizeof(aset_status));
			k_sleep(ASET_SLEEP_TIME);
			continue;
		} else {
			if (!ae_addr_configed) {
#ifdef ATS283F_SDK
				SYS_LOG_INF("To set ae Addr.");
				ret_val =
				    media_player_set_effect_addr(dump_player,
								 p_aset_debug_buffer);
#else
				ret_val = false;
#endif
				if (ret_val) {
					SYS_LOG_WRN("ret=%d", ret_val);
					continue;
				} else {
					ret_val = aset_read_data(STUB_CMD_ASET_READ_STATUS, &aset_status, sizeof(aset_status));
					if (aset_status.upload_case_info == TRUE) {
						aset_write_case_info(app_id);
					}
					/* update user dae again, as it's updated in
					   "media_service_open->music_effect_open->music_effect_updata_param_by_volume"
					   when pause/play the player.
					 */
					aset_volume_table_deal(app_id);
					aset_dae_deal();
					ae_addr_configed = true;
				}
			}
		}

		ret_val =
		    aset_read_data(STUB_CMD_ASET_READ_STATUS, &aset_status,
				   sizeof(aset_status));
		if (!ret_val) {
			err_cnt = 0;
			aset_cmd_deal(&aset_status, app_id);
		} else {
			SYS_LOG_WRN("read ret=%d", ret_val);
			if (err_cnt > 5) {
				break;
			} else {
				err_cnt++;
			}
		}

		ret_val = aset_get_volume_table_status();
		if (ret_val == 1) {
			aset_volume_table_deal(app_id);
		}

		k_sleep(ASET_SLEEP_TIME);
	}

 exit:
	dump_player = media_player_get_current_dumpable_player();
#ifdef ATS283F_SDK
	if (dump_player) {
		media_player_set_effect_addr(dump_player, NULL);
	}
#endif

	SYS_LOG_INF("free");
	if (NULL != p_aset_rw_buffer) {
		free(p_aset_rw_buffer);
		p_aset_rw_buffer = NULL;
	}

	if (NULL != p_aset_debug_buffer) {
		free(p_aset_debug_buffer);
		p_aset_debug_buffer = NULL;
	}
	if (NULL != p_volume_table) {
		free(p_volume_table);
		p_volume_table = NULL;
	}

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_unset_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "tool");
#endif

	SYS_LOG_INF("Exit");
}
