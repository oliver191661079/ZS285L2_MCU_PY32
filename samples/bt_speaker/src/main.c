/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <mem_manager.h>
#include <msg_manager.h>
#include <sys_manager.h>
#include <app_manager.h>
#include "app_defines.h"
#include "main/main_app.h"
#include "run_mode.h"
#include "app_tws.h"

extern int system_app_launch_init(void);

static void main_pre_init(void)
{
	struct app_msg msg;

	system_pre_init();

	run_mode_init();

#ifdef CONFIG_APP_TWS
	app_tws_load_tws_mode();
#endif

	//to run system app
	app_manager_active_app(CONFIG_SYS_APP_NAME);
	SYS_LOG_INF("Wait system app.\n");
	while(!system_is_ready()){
		os_sleep(10);
	}
	SYS_LOG_INF("System is ready.\n");

	system_app_launch_init();

	/*clear all remain message */
	while (receive_msg(&msg, OS_NO_WAIT)) {
		SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg.type, msg.cmd, msg.value);

		switch(msg.type){
#ifdef CONFIG_CHARGER_APP
			extern int charger_mode_check(void);
			case MSG_CHARGER_MODE:
				if(msg.cmd){
					charger_mode_check();
				}
				break;
#endif
			default:
				break;
		}

		if (msg.callback) {
			msg.callback(&msg, 0, NULL);
		}
	}

	memset(&msg, 0, sizeof(msg));
	msg.type = MSG_INIT_APP;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);

}

void main(void)
{
	SYS_LOG_INF("start\n");

	main_pre_init();

	main_app();
}
