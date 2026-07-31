/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief disk_ota_pre app main.
 */

#include "disk_ota_pre.h"
#include <ui_manager.h>
#include "app_launch.h"

static struct disk_ota_pre_app_t *p_disk_ota_pre = NULL;

struct disk_ota_pre_app_t *disk_ota_pre_get_app(void)
{
	return p_disk_ota_pre;
}

static void disk_ota_pre_uhost_start(void)
{
	struct app_msg msg = { 0 };

	msg.type = MSG_DISK_OTA_PRE_APP_EVENT;
	msg.cmd = MSG_DISK_OTA_PRE_UHOST_SCAN_FINISH;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

static void _disk_ota_pre_scan_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	usbh_scan_device();
	SYS_LOG_INF("uhost scan finish\n");
	disk_ota_pre_uhost_start();
}


static u8_t disk_ota_pre_scan_stack[CONFIG_USB_HOTPLUG_STACKSIZE]  __aligned(4) _NODATA_SECTION(.disk_ota.scan.bss);

static int _disk_ota_pre_usb_disk_scan(void)
{
	usbh_prepare_scan();
	p_disk_ota_pre->status = DISK_OTA_PRE_STATUS_SCAN;

	/* Start a thread to offload USB scan/enumeration */
	os_thread_create(disk_ota_pre_scan_stack, CONFIG_USB_HOTPLUG_STACKSIZE,
			_disk_ota_pre_scan_thread,
			NULL, NULL, NULL,
			6, 0, 0);

	return 0;
}


static int _disk_ota_pre_init(void *p1, void *p2, void *p3)
{

	u8_t ota_cnt = 0;

	if(p_disk_ota_pre)
		return 0;

	p_disk_ota_pre = app_mem_malloc(sizeof(struct disk_ota_pre_app_t));

	if(!p_disk_ota_pre){
		SYS_LOG_ERR("malloc err\n");
		return -1;
	}


#ifdef CONFIG_OTA_BACKEND_SDCARD
	if (hotplug_manager_get_state(HOTPLUG_SDCARD) == HOTPLUG_IN) {
		int ret = fs_manager_disk_init("SD:");
		if(!ret){
			ret = ota_app_init_sdcard();
			if(ret){
				goto exit;
			}else{
				ota_cnt ++;
				SYS_LOG_INF("start sdcard ota\n");
			}
		}
	}
#endif

#ifdef CONFIG_OTA_BACKEND_UHOST
	if (hotplug_manager_get_state(HOTPLUG_USB_HOST) == HOTPLUG_IN) {
		ota_cnt ++;
		if (usbh_is_scanning() == 2) {
			SYS_LOG_INF("start uhost disk ota\n");
			disk_ota_pre_uhost_start();
		}else{
			_disk_ota_pre_usb_disk_scan();
			goto exit;
		}
	}
#endif

exit:
	if(ota_cnt){
		SYS_LOG_INF("init ok\n");
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		p_disk_ota_pre->set_dvfs_level = DISK_OTA_PRE_DVFS_LEVEL;
		soc_dvfs_set_level(p_disk_ota_pre->set_dvfs_level, "disk_ota_pre");
#endif
	}else{
		desktop_manager_exit_app(DESKTOP_PLUGIN_ID_DISK_OTA_PRE);
		desktop_manager_del(DESKTOP_PLUGIN_ID_DISK_OTA_PRE);
	}
	return 0;
}

static int _disk_ota_pre_exit(void)
{
	disk_ota_pre_view_deinit();

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if (p_disk_ota_pre->set_dvfs_level) {
		soc_dvfs_unset_level(p_disk_ota_pre->set_dvfs_level, "disk_ota_pre");
		p_disk_ota_pre->set_dvfs_level = 0;
	}
#endif

	app_mem_free(p_disk_ota_pre);
	p_disk_ota_pre = NULL;

	SYS_LOG_INF("exit ok\n");

	desktop_manager_exit_app(DESKTOP_PLUGIN_ID_DISK_OTA_PRE);
	desktop_manager_del(DESKTOP_PLUGIN_ID_DISK_OTA_PRE);

	return 0;
}

static int _disk_ota_pre_proc_msg(struct app_msg *msg)
{
	int ret = 0;

	SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg->type, msg->cmd,
		    msg->value);
	switch (msg->type) {
	case MSG_DISK_OTA_PRE_APP_EVENT:
		{
			if(msg->cmd == MSG_DISK_OTA_PRE_UHOST_SCAN_FINISH)
			{
				if (usbh_is_scanning() == 2) {
					int res = fs_manager_disk_init("USB:");
					if(res){
						SYS_LOG_INF("disk_init failed %d\n", ret);
						desktop_manager_exit_app(DESKTOP_PLUGIN_ID_DISK_OTA_PRE);
						desktop_manager_del(DESKTOP_PLUGIN_ID_DISK_OTA_PRE);
					}

					ret = ota_app_init_uhost();
					if(!ret){
						SYS_LOG_INF("start uhost ota\n");
						break;
					}
				}
				desktop_manager_exit_app(DESKTOP_PLUGIN_ID_DISK_OTA_PRE);
				desktop_manager_del(DESKTOP_PLUGIN_ID_DISK_OTA_PRE);
			}
		}
		break;

	case MSG_EXIT_APP:
		_disk_ota_pre_exit();
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_DISK_OTA_PRE, _disk_ota_pre_init, _disk_ota_pre_exit,
		      _disk_ota_pre_proc_msg, NULL, NULL, NULL, NULL);
