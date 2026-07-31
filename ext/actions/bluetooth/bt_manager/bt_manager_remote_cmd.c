/*
 * Copyright (c) 2026 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bt_manager_remote_cmd.c
 *
 * @brief 远程控制命令统一分发（UART / SPP 等入口共用）。
 *
 * 载荷格式：首字节 0xD2，次字节为子命令（与 bt_manager_uart_rx 一致）。
 * 0x00~0x09 媒体/音量/通话；0x0A 重启进入 ADFU。
 */

#define SYS_LOG_DOMAIN "bt_remote_cmd"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INF
#include <logging/sys_log.h>

#include <zephyr.h>
#include <misc/printk.h>
#include <errno.h>
#include <string.h>

#include <bt_manager.h>
#include <bt_manager_audio.h>
#include <soc.h>

static void remote_cmd_log_ret(const char *src, const char *api, int ret)
{
	if (!src) {
		src = "?";
	}
	if (ret == 0) {
		printk("[remote_cmd] src=%s %s ret=0 (ok)\n", src, api);
	} else {
		printk("[remote_cmd] src=%s %s ret=%d (fail)\n", src, api, ret);
	}
}

int bt_manager_remote_cmd_dispatch(const uint8_t *payload, uint16_t len, const char *src)
{
	int ret;

	if (!payload || len < 2U || payload[0] != 0xD2u) {
		printk("[remote_cmd] src=%s skip: len=%u need D2+subcmd\n",
		       src ? src : "?", (unsigned int)len);
		return -EINVAL;
	}

	printk("[remote_cmd] src=%s cmd D2 sub=0x%02x\n", src ? src : "?",
	       (unsigned int)payload[1]);

	switch (payload[1]) {
	case 0x00:
		ret = bt_manager_media_play_previous();
		remote_cmd_log_ret(src, "media_play_previous", ret);
		return ret;
	case 0x01:
		ret = bt_manager_media_play_next();
		remote_cmd_log_ret(src, "media_play_next", ret);
		return ret;
	case 0x02:
		ret = bt_manager_media_pause();
		remote_cmd_log_ret(src, "media_pause", ret);
		return ret;
	case 0x03:
		ret = bt_manager_media_play();
		remote_cmd_log_ret(src, "media_play", ret);
		return ret;
	case 0x04:
		ret = bt_manager_media_playpause();
		remote_cmd_log_ret(src, "media_playpause", ret);
		return ret;
	case 0x05:
		ret = bt_manager_volume_up();
		remote_cmd_log_ret(src, "volume_up", ret);
		return ret;
	case 0x06:
		ret = bt_manager_volume_down();
		remote_cmd_log_ret(src, "volume_down", ret);
		return ret;
	case 0x07:
		ret = bt_manager_audio_effect_switch();
		remote_cmd_log_ret(src, "audio_effect_switch", ret);
		return ret;
	case 0x08:
		ret = bt_manager_call_accept(NULL);
		remote_cmd_log_ret(src, "call_accept", ret);
		return ret;
	case 0x09:
		ret = bt_manager_call_terminate(NULL, 0);
		remote_cmd_log_ret(src, "call_terminate", ret);
		return ret;
	case 0x0A:
		printk("[remote_cmd] src=%s reboot to ADFU\n", src ? src : "?");
		sys_pm_reboot(REBOOT_TYPE_GOTO_ADFU);
		return 0;
	default:
		printk("[remote_cmd] src=%s unknown subcmd 0x%02x\n",
		       src ? src : "?", (unsigned int)payload[1]);
		return -ENOTSUP;
	}
}
