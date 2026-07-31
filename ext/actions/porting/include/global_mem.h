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
 *	2017/1/20: Created by wh.
 */

#ifndef __GLOBAL_MEM_H__
#define __GLOBAL_MEM_H__

#include <os_common_api.h>
#include "mem_manager.h"

/* size of stack area used by mqtt services thread */
#ifdef CONFIG_MQTT_STACKSIZE
#define MQTT_STACKSIZE CONFIG_MQTT_STACKSIZE
#else
#define MQTT_STACKSIZE 0
#endif

/*mqtt services stack  for thread*/
extern char __noinit  __aligned(STACK_ALIGN) mqtt_service_stack_area[MQTT_STACKSIZE];

/* size of stack area used by each thread */
#ifdef CONFIG_MUSIC_STACKSIZE
#define MUSIC_STACKSIZE CONFIG_MUSIC_STACKSIZE
#else
#define MUSIC_STACKSIZE 0
#endif
/*share stack for music encoder and decoder  thread */
extern char __noinit  __aligned(STACK_ALIGN) music_share_stack_area[];

/* size of stack area used by each thread */
#ifdef CONFIG_MEDIASRV_STACKSIZE
#define MEDIASRV_STACKSIZE CONFIG_MEDIASRV_STACKSIZE
#else
#define MEDIASRV_STACKSIZE 0
#endif
/*share stack for music encoder and decoder  thread */
extern char __noinit  __aligned(STACK_ALIGN) meidasrv_share_stack_area[];




/* size of stack area used by download thread */
#ifdef CONFIG_DOWNLOAD_STACKSIZE
#define DOWNLOAD_STACKSIZE CONFIG_DOWNLOAD_STACKSIZE
#else
#define DOWNLOAD_STACKSIZE 0
#endif

/*download music thread */
extern char __noinit  __aligned(STACK_ALIGN) download_stack_area[DOWNLOAD_STACKSIZE];

/* size of stack area used by user work queue thread */
#ifdef CONFIG_USER_WORK_Q
#ifdef CONFIG_USER_WORK_Q_STACK_SIZE
#define USER_WQ_STACK_SIZE CONFIG_USER_WORK_Q_STACK_SIZE
#else
#define USER_WQ_STACK_SIZE 0
#endif
/** user work queue tread */
extern char __noinit  __aligned(STACK_ALIGN) user_work_q_stack[USER_WQ_STACK_SIZE];
#endif

#ifdef CONFIG_USED_MEM_POOL
extern struct k_mem_pool app_mem_pool;
#endif

#ifdef CONFIG_USED_MEM_SLAB
extern const struct slabs_info system_slab;
extern const struct slabs_info app_slab;
#endif
#endif
