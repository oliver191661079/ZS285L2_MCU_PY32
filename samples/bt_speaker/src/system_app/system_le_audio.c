#include <logging/sys_log.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bt_manager.h>
#include <broadcast.h>

#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif
#include "system_app.h"

#ifdef CONFIG_BT_LE_AUDIO_CALL
#define DEFAULT_CONTEXTS	(BT_AUDIO_CONTEXT_UNSPECIFIED | \
				BT_AUDIO_CONTEXT_CONVERSATIONAL | \
				BT_AUDIO_CONTEXT_MEDIA|BT_AUDIO_CONTEXT_GAME| \
				BT_AUDIO_CONTEXT_SOUND_EFFECTS|BT_AUDIO_CONTEXT_NOTIFICATIONS| \
				BT_AUDIO_CONTEXT_RINGTONE|BT_AUDIO_CONTEXT_ALERTS | \
				BT_AUDIO_CONTEXT_EMERGENCY_ALARM)
#else
#define DEFAULT_CONTEXTS	(BT_AUDIO_CONTEXT_UNSPECIFIED | \
				BT_AUDIO_CONTEXT_MEDIA|BT_AUDIO_CONTEXT_GAME| \
				BT_AUDIO_CONTEXT_SOUND_EFFECTS|BT_AUDIO_CONTEXT_NOTIFICATIONS| \
				BT_AUDIO_CONTEXT_RINGTONE|BT_AUDIO_CONTEXT_ALERTS | \
				BT_AUDIO_CONTEXT_EMERGENCY_ALARM)
#endif

static uint16_t sink_available_contexts;
static uint16_t source_available_contexts;

extern void bt_manager_le_audio_get_sirk(uint8_t *p_sirk);

static uint32_t sink_location = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;

static int leaudio_cap_init(void)
{
	struct bt_audio_capabilities_2 caps = {0};
	struct bt_audio_capability *cap;

	caps.source_cap_num = 1;
	caps.sink_cap_num = 1;

	/* Audio Source */
	cap = &caps.cap[0];
	cap->format = BT_AUDIO_CODEC_LC3;
	cap->channels = BT_AUDIO_CHANNEL_COUNTS_1;
	cap->frames = 1;
	cap->samples = bt_supported_sampling_from_khz(16) |
			bt_supported_sampling_from_khz(32);
	cap->octets_min = 40;
	cap->octets_max = 80;
	cap->durations = BT_SUPPORTED_FRAME_DURATION_10MS;
	cap->preferred_contexts = source_available_contexts;
	caps.source_available_contexts = source_available_contexts;
	caps.source_supported_contexts = 0xFFF;
	caps.source_locations = 0;

	/* Audio Sink */
	cap = &caps.cap[1];

	cap->format = BT_AUDIO_CODEC_LC3;
	cap->channels = BT_AUDIO_CHANNEL_COUNTS_1;

	if ((sink_location == BT_AUDIO_LOCATIONS_FR) ||
		(sink_location == BT_AUDIO_LOCATIONS_FL)) {
		cap->frames = 1;
	}else {
		cap->frames = 2;
	}
	cap->samples = bt_supported_sampling_from_khz(16) |
			bt_supported_sampling_from_khz(24) |
			bt_supported_sampling_from_khz(32) |
			bt_supported_sampling_from_khz(48);
	cap->octets_min = 40;
	cap->octets_max = 155;
	cap->durations = BT_SUPPORTED_FRAME_DURATION_10MS;
	cap->preferred_contexts = sink_available_contexts;
	caps.sink_available_contexts = sink_available_contexts;
	caps.sink_supported_contexts = 0xFFF;

	if ((sink_location == BT_AUDIO_LOCATIONS_FR) ||
		(sink_location == BT_AUDIO_LOCATIONS_FL)) {
		caps.sink_locations = sink_location;
	}else {
		caps.sink_locations = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;
	}

	SYS_LOG_INF("samples: 0x%x, channels: %d, frames: %d, sink_locations: %d\n",
		cap->samples, cap->channels, cap->frames, caps.sink_locations);

	return bt_manager_audio_server_cap_init((struct bt_audio_capabilities *)&caps);
}

static int leaudio_qos_init(void)
{
	struct bt_audio_preferred_qos_default qos;
	uint32_t delay;

	qos.framing = BT_AUDIO_UNFRAMED_SUPPORTED;
	qos.phy = BT_AUDIO_PHY_2M;
	qos.rtn = 13;
	qos.latency = 100;

	/* Audio Source */
	delay = 10000;
	delay += 2000;	// TODO: optimize
	qos.delay_min = delay;
	qos.delay_max = 40000;	/* 40ms at most */
	qos.preferred_delay_min = delay;
	qos.preferred_delay_max = 40000;	/* 40ms at most */

	bt_manager_audio_server_qos_init(true, &qos);

	/* Audio Sink */
	delay = 20000;
	qos.delay_min = delay;
	qos.delay_max = 40000;	/* 40ms at most */
	qos.preferred_delay_min = delay;
	qos.preferred_delay_max = 40000;	/* 40ms at most */

	bt_manager_audio_server_qos_init(false, &qos);

	return 0;
}

int bt_le_audio_init(void)
{
	struct bt_audio_role role = {0};

#ifdef CONFIG_BT_LE_AUDIO
	/* lea need to be on by default, to support OTA from old verison.*/
	role.lea_connection_enable = 1;
#ifdef CONFIG_PROPERTY
	role.lea_connection_enable = property_get_int(CFG_LEA_ONOFF_STATUS, 1);
	SYS_LOG_INF("lea onff status %d\n",role.lea_connection_enable);
#endif

#ifdef CONFIG_BT_LEATWS
	extern uint32_t leatws_get_audio_channel(void);
	sink_location = leatws_get_audio_channel();
#endif

#ifdef CONFIG_BT_LE_AUDIO_MASTER
	/* master */
	role.num_master_conn = 1;
	role.unicast_client = 1;
	role.volume_controller = 1;
	role.media_device = 1;
	role.disable_name_match = 1;
#endif

#ifdef CONFIG_BT_LETWS
    role.num_master_conn = 1;
	role.disable_name_match = 1;
#endif

	/* slave */
	role.num_slave_conn = 3;
	role.unicast_server = 1;
	role.target_announcement = 1;
	role.volume_renderer = 1;
	role.media_controller = 1;
#ifdef CONFIG_BT_LE_AUDIO_CALL
	role.microphone_device = 1;
	role.call_terminal = 1;
#endif

	role.set_member = 1;

	if (sink_location == BT_AUDIO_LOCATIONS_FR) {
		role.set_rank = 2;
	}else {
		role.set_rank = 1;
	}

	if ((sink_location == BT_AUDIO_LOCATIONS_FR) ||
		(sink_location == BT_AUDIO_LOCATIONS_FL)) {
		role.set_size = 2;
	}else {
		role.set_size = 1;
	}
#endif /*CONFIG_BT_LE_AUDIO*/

	role.encryption = 1;
#ifdef CONFIG_BT_LETWS
#ifndef CONFIG_BT_SMP_MASTER_DISALLOWED
	if (role.encryption) {
	    role.master_encryption = 1;
	}
#endif	
#endif
	/*
	 * broadcast: support broadcast_source and broadcast_sink
	 */
	role.broadcast_sink = 1;
	role.broadcast_source = 1;
    role.scan_delegator = 1;

	role.sirk_enabled = 1;
	bt_manager_le_audio_get_sirk(role.set_sirk);

	sink_available_contexts = DEFAULT_CONTEXTS;
#ifdef CONFIG_BT_LE_AUDIO_CALL
	source_available_contexts = DEFAULT_CONTEXTS;
#else
	source_available_contexts = BT_AUDIO_CONTEXT_UNSPECIFIED | BT_AUDIO_CONTEXT_GAME;
#endif

#if 0
	if (role.gmas_config.gmas_role.role_ugt) {
		if (role.gmas_config.feat.ugt_feat_sink) {
			sink_available_contexts |= BT_AUDIO_CONTEXT_GAME;
		} else {
			sink_available_contexts = 0;
		}
		if (role.gmas_config.feat.ugt_feat_source) {
			source_available_contexts |= BT_AUDIO_CONTEXT_UNSPECIFIED |
					BT_AUDIO_CONTEXT_GAME |
					BT_AUDIO_CONTEXT_MEDIA;
		} else {
			source_available_contexts = 0;
		}
	}
#endif
	role.sink_available_contexts = sink_available_contexts;
	role.source_available_contexts = source_available_contexts;

	SYS_LOG_INF("rank: 0x%x, size: %d context %x %x \n",role.set_rank, role.set_size,sink_available_contexts
		,source_available_contexts);

	bt_manager_audio_init(BT_TYPE_LE, &role);
	bt_manager_lea_policy_ctx_init(role.lea_connection_enable);

	leaudio_cap_init();
	leaudio_qos_init();

#ifdef CONFIG_BT_LETWS
    bt_manager_audio_le_pause_scan();

	struct bt_le_conn_param conn_param;
	/* 30ms interval, 8s timeout */
	conn_param.interval_min = 24;
	conn_param.interval_max = 24;
	conn_param.latency = 0;
	conn_param.timeout = 800;
	bt_manager_audio_le_conn_param_init(&conn_param);

#endif

	bt_manager_audio_start(BT_TYPE_LE);

#ifdef CONFIG_BT_LETWS
    broadcast_tws_init_group_info();
	bt_manager_init_letws_info(broadcast_tws_vnd_rx_cb);
#endif

	return 0;
}

int bt_le_audio_exit(void)
{
	SYS_LOG_INF("\n");

	bt_manager_audio_stop(BT_TYPE_LE);

	bt_manager_audio_exit(BT_TYPE_LE);

	return 0;
}
