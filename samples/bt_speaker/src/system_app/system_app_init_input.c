/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <misc/util.h>
#include <soc.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <sys_monitor.h>
#include <sys_wakelock.h>
#include <ui_manager.h>
#include <app_manager.h>
#include <input_manager.h>
#include <sys_manager.h>
#ifdef CONFIG_ESD_MANAGER
#include <esd_manager.h>
#endif
#include "system_app.h"

#ifdef CONFIG_INPUT_MANAGER

void _system_key_event_cb(u32_t key_value, uint16_t type)
{
	if ((key_value & KEY_TYPE_DOUBLE_CLICK) == KEY_TYPE_DOUBLE_CLICK
	    || (key_value & KEY_TYPE_TRIPLE_CLICK) == KEY_TYPE_TRIPLE_CLICK
	    || (key_value & KEY_TYPE_SHORT_UP) == KEY_TYPE_SHORT_UP
	    || (key_value & KEY_TYPE_LONG_DOWN) == KEY_TYPE_LONG_DOWN) {
		/** need process key tone */
#ifdef CONFIG_PLAY_KEYTONE
		//key_tone_play();
#endif
	}

	/*filter onoff key for power on */
	if (((key_value & ~KEY_TYPE_ALL) == KEY_POWER)
	    && (k_uptime_get() < 1800)) {
		input_manager_filter_key_itself();
	}

}

void system_input_handle_init(void)
{
#ifdef CONFIG_INPUT_MANAGER
	input_manager_init(_system_key_event_cb);
	/** input init is locked ,so we must unlock*/
	input_manager_unlock();
#endif
}
#endif
