/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bmr.h"
#include "broadcast.h"
#include <app_launch.h>

static struct bmr_app_t *p_bmr = NULL;

static int bmr_vnd_ext_rx(uint32_t handle, const uint8_t *buf, uint16_t len)
{
	SYS_LOG_INF("h: 0x%x, l: %d, v: 0x%x 0x%x 0x%x 0x%x\n",
		handle, len, buf[0], buf[1], buf[2], buf[3]);

#ifdef CONFIG_AUARCAST_FILTER_POLICY_H
    if (buf[0] != (COMPANY_ID & 0xff) || buf[1] != ((COMPANY_ID >> 8) & 0xff) ||
        buf[18] != (SERIVCE_UUID & 0xff) || buf[19] != ((SERIVCE_UUID >> 8) & 0xff))
    {
        SYS_LOG_ERR("COMPANY_ID or SERVICE_UUID compare fail\n");
        return -1;
    }
#endif

#if ENABLE_ENCRYPTION
	if ((buf[0] == (VS_ID & 0xFF)) &&
		(buf[1] == (VS_ID >> 8)) &&
		(buf[2] != 0)) {
		p_bmr->encryption = 1;
		memcpy(p_bmr->broadcast_code, &buf[2], 16);

		SYS_LOG_INF("encryption\n");
	}
#endif

	return 0;
}

#if SCAN_FILTER_BY_USER
static int bmr_user_scan_filter_cb(struct btsrv_broadcast_info *info)
{
    printk("local name: %s\n",info->local_name);
    printk("broadcast name: %s\n",info->broadcast_name);
    printk("broadcast id %d\n",info->broadcast_id);
    printk("company_id %d\n",info->company_id);
    printk("uuid16 %d\n",info->uuid16);
    printk("rssi %d\n",info->rssi);

    /* match */
    return 0;
}
#endif

static int bmr_sink_init(void)
{
	uint32_t value;


	value = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;

	bt_manager_broadcast_sink_init();
#if BIS_SYNC_LOCATIONS
	bt_manager_broadcast_sink_sync_loacations(value);
#endif

	if (false == bmr_any_get()) {
#if FILTER_LOCAL_NAME
		bt_manager_broadcast_filter_local_name(broadcast_get_local_name());
#endif
#if FILTER_BROADCAST_NAME
		bt_manager_broadcast_filter_broadcast_name(broadcast_get_broadcast_name());
#endif
#if FILTER_COMPANY_ID
		bt_manager_broadcast_filter_company_id(VS_ID);
#endif
#if FILTER_UUID16
		bt_manager_broadcast_filter_uuid16(VS_ID);
#endif

#if FILTER_BROADCAST_ID
		bt_manager_broadcast_filter_broadcast_id(broadcast_get_broadcast_id());
#endif
	} else {
		bt_manager_broadcast_filter_local_name(NULL);
		bt_manager_broadcast_filter_broadcast_name(NULL);
		bt_manager_broadcast_filter_company_id(0x10000);
		bt_manager_broadcast_filter_uuid16(0);
		bt_manager_broadcast_filter_broadcast_id(INVALID_BROADCAST_ID);
	}

#if SCAN_FILTER_BY_USER
    bt_manager_broadcast_register_scan_filter_cb(bmr_user_scan_filter_cb);
#endif

	bt_manager_broadcast_sink_register_ext_rx_cb(bmr_vnd_ext_rx);
	return bt_manager_broadcast_scan_start();

	return 0;
}

static int bmr_sink_exit(void)
{
	bt_manager_broadcast_sink_exit();

	//bt_manager_pawr_receive_stop();
	//bt_manager_pawr_adv_stop();

	return 0;
}

static void bmr_clean_broad_dev(void)
{
	struct bmr_app_t *bmr = bmr_get_app();
	struct bmr_broad_dev *dev,*tmp;

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&bmr->broad_dev_list, dev, tmp, node) {
		SYS_LOG_INF("hdl:0x%x\n",dev->handle);
		sys_slist_find_and_remove(&bmr->broad_dev_list, &dev->node);
		mem_free(dev);
	}
}

static int _bmr_init(void *p1, void *p2, void *p3)
{
	if (p_bmr) {
		return 0;
	}

	SYS_LOG_INF("in");

	p_bmr = app_mem_malloc(sizeof(struct bmr_app_t));
	if (!p_bmr) {
		SYS_LOG_ERR("malloc failed!\n");
		return -ENOMEM;
	}
	memset(p_bmr, 0, sizeof(struct bmr_app_t));

#ifdef CONFIG_PLAYTTS
	if(tts_manager_is_playing()){
		p_bmr->tts_playing = 1;
	}
#endif

	bmr_view_init();
	bmr_sink_init();

	system_app_set_auracast_mode(2);

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_set_level(BMR_FREQ, "bmr");
#endif
	SYS_LOG_INF("init ok\n");
	return 0;
}

static int _bmr_exit(void)
{
	if (!p_bmr) {
		goto exit;
	}

	SYS_LOG_INF("in");

	bmr_stop_playback();
	bmr_exit_playback();

	bmr_sink_exit();

	bmr_view_deinit();

	bmr_clean_broad_dev();
	app_mem_free(p_bmr);

	p_bmr = NULL;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_unset_level(BMR_FREQ, "bmr");
#endif

exit:
	SYS_LOG_INF("exit ok\n");

	return 0;
}

struct bmr_app_t *bmr_get_app(void)
{
	return p_bmr;
}

static int _bmr_proc_msg(struct app_msg *msg)
{
	switch (msg->type) {
	case MSG_BT_EVENT:
		bmr_bt_event_proc(msg);
		break;
	case MSG_INPUT_EVENT:
		bmr_input_event_proc(msg);
		break;
#ifdef CONFIG_PLAYTTS
	case MSG_TTS_EVENT:
		bmr_tts_event_proc(msg);
#endif
		break;
	case MSG_EXIT_APP:
		_bmr_exit();
		break;
	default:
		break;
	}

	return 0;
}

static int _bmr_dump_app_state(void)
{
	print_buffer_lazy(APP_ID_BMR, (void *)p_bmr, sizeof(struct bmr_app_t));
	return 0;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_BMR, _bmr_init, _bmr_exit, _bmr_proc_msg, \
	_bmr_dump_app_state, NULL, NULL, NULL);
