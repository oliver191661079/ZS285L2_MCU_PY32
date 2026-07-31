/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief card reader app view
*/

#include "charger.h"
#include "hotplug_manager.h"
#include "ui_manager.h"
#include <input_manager.h>
#include <ui_manager.h>


static int _charger_view_proc(u8_t view_id, u8_t msg_id, u32_t msg_data)
{
    switch (msg_id)
    {
        case MSG_VIEW_CREATE:
        {
			SYS_LOG_INF("CREATE \n");
		#ifdef CONFIG_SEG_LED_MANAGER
		#ifdef CONFIG_HOTPLUG
			seg_led_display_string(SLED_NUMBER1, "CHAR", true);
		#endif
		#endif
		#ifdef CONFIG_LED_MANAGER
			led_manager_set_display(0, LED_ON, OS_FOREVER, NULL);
			led_manager_set_display(1, LED_ON, OS_FOREVER, NULL);
		#endif
            break;
        }
        case MSG_VIEW_DELETE:
		{
			SYS_LOG_INF(" DELETE \n");
			break;
		}
		case MSG_VIEW_PAINT:
		{
			break;
		}
    }
    return 0;
}

void charger_view_init(void)
{
	ui_view_info_t  view_info;

	memset(&view_info, 0, sizeof(ui_view_info_t));
	view_info.view_proc = _charger_view_proc;
	view_info.order = 1;
	view_info.app_id = APP_ID_MAIN;

	ui_view_create(CHARGER_VIEW, &view_info);

	SYS_LOG_INF("ok\n");
}

void charger_view_deinit(void)
{
	ui_view_delete(CHARGER_VIEW);

	SYS_LOG_INF("ok\n");
}
