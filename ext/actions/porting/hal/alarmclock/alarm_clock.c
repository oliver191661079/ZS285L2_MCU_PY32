/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * Author: wh<wanghui@actions-semi.com>
 *
 * Change log:
 *	2017/3/15: Created by wh.
 */
#include <os_common_api.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <rtc.h>
#ifdef CONFIG_NETWORKING
#include <net/net_ip.h>
#include <net/net_core.h>
#include <net/http_parser.h>
#endif
#include "http_client.h"
#include "tcp_client.h"
#include "stream.h"
#include "mem_manager.h"
#include "alarm_manager.h"
#include "system_config.h"

const char clock_url[] = "http://www.baidu.com";
const char DATA_TOKEN[] = "Date:";
const char DATA_END_TOKEN[] = "GMT";

K_SEM_DEFINE(clock_sem, 0, 1);

#define MAX_RECEIVED_BUFFER_SIZE 512

static struct alarm_manager al_manager;

uint32_t system_init_timestamp = 0;

alarm_callback call_back = NULL;

static void clock_response(struct tcp_client_ctx *tcp_ctx, struct net_pkt *rx)
{

	uint16_t nbuffer_off = 0 ,nbuffer_len = 0;
	char * rec_buff = NULL ;
	char * data_token = NULL;
	char * data_end = NULL;

	if (!rx)
	{
		goto exit; 
	}

	nbuffer_off = net_pkt_get_len(rx) - net_pkt_appdatalen(rx);
	nbuffer_len = net_pkt_appdatalen(rx);

	rx->frags->data[nbuffer_off + nbuffer_len] = 0;

	rec_buff = &rx->frags->data[nbuffer_off];

	data_token = strstr(rec_buff,DATA_TOKEN);

	if(data_token != NULL)
	{
		data_end = strstr(data_token,DATA_END_TOKEN);
		data_end[3] = 0;
		printk("data_token %s \n",data_token);
		system_init_timestamp = GET_UNIX_TIME_STAMP(data_token + strlen(DATA_TOKEN));
	}

exit:
	if(rx) {
		net_pkt_unref(rx);
	}
	k_sem_give(&clock_sem);
}

int system_clock_init(void)
{
	int res = 0;
	struct http_client_ctx *http_ctx;
	struct request_info request;

	http_ctx = http_init();
	if (!http_ctx) {
		goto exit;
	}

	memset(&request, 0, sizeof(struct request_info));
	request.url = (char *)clock_url;

	http_ctx->tcp_ctx->receive_cb = clock_response;

	res = http_send_get(http_ctx, &request);

	if(res == 0) {
		k_sem_take(&clock_sem,K_MSEC(10000));
	}

	tcp_disconnect(http_ctx->tcp_ctx);
	http_deinit(http_ctx);
exit:
	return system_init_timestamp;
}

static void find_and_set_alarm(void)
{
	int i = 0;
	int earliest_alarm = 0;
	struct alarm_info * alarm = NULL;
	struct device *rtc = device_get_binding(RTC_DEVICE_NAME);

	if(rtc == NULL)
	{
		SYS_LOG_ERR("no rtc\n");
		return ;
	}

	for(i = 0 ; i < MAX_ALARM_SUPPORT ; i++)
	{
		alarm = &al_manager.alarm[i];

		if(alarm->state == ALARM_STATE_OK && alarm->alarm_time > rtc_read(rtc))
		{
			if(earliest_alarm == 0)
			{
				earliest_alarm = alarm->alarm_time;
			}
			else if(earliest_alarm > alarm->alarm_time)
			{
				earliest_alarm = alarm->alarm_time;
			}
		}
	}
	if(earliest_alarm != 0)
	{
		if (rtc_set_alarm(rtc, earliest_alarm)) {
			SYS_LOG_ERR("Failed to set RTC Alarm\n");
		}
	}

	system_delete_overdue_alarm(earliest_alarm);
}

static void rtc_alarm_callback(struct device *rtc_dev)
{
	SYS_LOG_INF("RTC counter: %u\n", rtc_read(rtc_dev));

	/* Verify rtc_get_pending_int() */
	if (rtc_get_pending_int(rtc_dev)) {
		SYS_LOG_INF("Catch pending RTC interrupt\n");
	} else {
		SYS_LOG_INF("Fail to catch pending RTC interrupt\n");
	}
	if(	call_back != NULL)
	{
		call_back();
	}
	find_and_set_alarm();
}


int alarm_manager_init(void)
{
	int ret = 0;
	struct rtc_config config;
	struct device *rtc = NULL;

	rtc = device_get_binding("RTC_0");


	system_init_timestamp = 0;

	config.init_val = system_clock_init();
	config.alarm_enable = 1;
	config.alarm_val = 0;
	config.cb_fn = rtc_alarm_callback;

	if(rtc == NULL)
	{
		SYS_LOG_ERR("no rtc\n");
		return -1;
	}

	rtc_enable(rtc);

	if (rtc_set_config(rtc, &config)) {
		SYS_LOG_ERR("Failed to config RTC alarm\n");
		return -1;
	}

	memset(&al_manager, 0, sizeof(struct alarm_manager));
#ifdef CONFIG_NVRAM_CONFIG
	ret = nvram_config_get(CFG_ALARM_INFO, &al_manager, sizeof(struct alarm_manager));
#endif
	if (ret < 0)
	{
       SYS_LOG_ERR("failed to get config %s, ret %d\n", CFG_ALARM_INFO, ret);
       return -1;
    }

	return 0;
}

int system_add_alarm(struct alarm_info * alarm)
{
	int ret = 0;
	int i = 0;
	struct alarm_info * tmp_alarm = NULL;

	SYS_LOG_INF("id %d alarm_time %d mode %d \n", alarm->id, alarm->alarm_time,alarm->mode);

	for(i = 0 ; i < MAX_ALARM_SUPPORT ; i++)
	{
		tmp_alarm = &al_manager.alarm[i];

		if(tmp_alarm->state == ALARM_STATE_FREE)
		{
			break;
		}
	}

	if(tmp_alarm == NULL)
	{
		return -2;
	}

	tmp_alarm->id = alarm->id;
	tmp_alarm->alarm_time = alarm->alarm_time;
	tmp_alarm->mode = alarm->mode;
	tmp_alarm->state = ALARM_STATE_OK;

#ifdef CONFIG_NVRAM_CONFIG
	ret = nvram_config_set(CFG_ALARM_INFO, &al_manager, sizeof(struct alarm_manager));
	if (ret < 0)
	{
       SYS_LOG_ERR("failed to set config %s, ret %d\n", CFG_ALARM_INFO, ret);
       return -1;
    }
#endif

	return 0;
}

int system_delete_overdue_alarm(int alarm_time)
{
	int i = 0;
	int ret = 0;
	bool changed = false;
	struct alarm_info * alarm = NULL;

	for(i = 0 ; i < MAX_ALARM_SUPPORT ; i++)
	{
		alarm = &al_manager.alarm[i];

		if(alarm->alarm_time <= alarm_time && alarm->state == ALARM_STATE_OK)
		{
			alarm->state = ALARM_STATE_FREE;
			changed = true;
		}
	}
	if(changed)
	{
	#ifdef CONFIG_NVRAM_CONFIG
		ret = nvram_config_set(CFG_ALARM_INFO, &al_manager, sizeof(struct alarm_manager));
	#endif
		if (ret < 0)
		{
	       SYS_LOG_ERR("failed to set config %s, ret %d\n", CFG_ALARM_INFO, ret);
	       return -1;
	    }
	}

	return 0;
}

int system_delete_alarm(int alarm_id)
{
	int i = 0;
	int ret = 0;
	bool changed = false;
	struct alarm_info * alarm = NULL;

	SYS_LOG_INF("alarm_id %d \n", alarm_id);

	for(i = 0 ; i < MAX_ALARM_SUPPORT ; i++)
	{
		alarm = &al_manager.alarm[i];

		if(alarm->id == alarm_id && alarm->state == ALARM_STATE_OK)
		{
			alarm->state = ALARM_STATE_FREE;
			changed = true;
		}
	}
	if(changed)
	{
	#ifdef CONFIG_NVRAM_CONFIG
		ret = nvram_config_set(CFG_ALARM_INFO, &al_manager, sizeof(struct alarm_manager));
	#endif
		if (ret < 0)
		{
	       SYS_LOG_ERR("failed to set config %s, ret %d\n", CFG_ALARM_INFO, ret);
	       return -1;
	    }
	}
	return 0;
}

int system_modify_alarm(struct alarm_info * alarm)
{
	int i = 0;
	int ret = 0;
	struct alarm_info * tmp_alarm = NULL;

	SYS_LOG_INF("id %d alarm_time %d mode %d \n", alarm->id, alarm->alarm_time,alarm->mode);

	for(i = 0 ; i < MAX_ALARM_SUPPORT ; i++)
	{
		alarm = &al_manager.alarm[i];

		if(alarm->id == tmp_alarm->id && tmp_alarm->state == ALARM_STATE_OK)
		{
			tmp_alarm->alarm_time = alarm->alarm_time;
		#ifdef CONFIG_NVRAM_CONFIG
			ret = nvram_config_set(CFG_ALARM_INFO, &al_manager, sizeof(struct alarm_manager));
			if (ret < 0)
			{
		       SYS_LOG_ERR("failed to set config %s, ret %d\n", CFG_ALARM_INFO, ret);
		       return -1;
		    }
		#endif
			return 0;
		}
	}
	return -1;
}

bool check_alarm(void)
{
	struct device *rtc = NULL;
	uint32_t timestamp = 0;
	struct alarm_info * alarm = NULL;
	bool alarmed = false;
	bool needupdate = false;
	int i = 0;

	if(system_init_timestamp == 0)
	{
		return alarmed;
	}

	rtc = device_get_binding("RTC_0");

	if(rtc != NULL)
	{
		timestamp = rtc_read(rtc);
		SYS_LOG_INF("timestamp %d \n",timestamp);
	}
	else
	{
		return alarmed;
	}

	for(i = 0 ; i < MAX_ALARM_SUPPORT ; i++)
	{
		alarm = &al_manager.alarm[i];
		if(alarm->state == ALARM_STATE_OK)
		{
			SYS_LOG_INF(" alarm_time : %d ",alarm->alarm_time);
		}

		if(alarm->alarm_time - timestamp <= 60 
			&& alarm->state == ALARM_STATE_OK)
		{
			alarm->state = ALARM_STATE_FREE;
			alarmed = true;
			needupdate = true;
			SYS_LOG_INF("alarmed ................ \n");
		}

		if(alarm->alarm_time <= timestamp && alarm->state == ALARM_STATE_OK)
		{
			SYS_LOG_INF("_overdue_alarm ................ \n");
			alarm->state = ALARM_STATE_FREE;
			alarmed = true;
			needupdate = true;
		}
	}
	if(needupdate)
	{
	#ifdef CONFIG_NVRAM_CONFIG
		i = nvram_config_set(CFG_ALARM_INFO, &al_manager, sizeof(struct alarm_manager));
		if (i < 0)
		{
	       SYS_LOG_ERR("failed to set config %s, ret %d\n", CFG_ALARM_INFO, i);
	    }
	#endif
	}

	return alarmed;
}

int system_registry_alarm_callback(alarm_callback callback)
{
	call_back = callback;
	return 0;
}
