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
 *	2017/7/7: Created by wh.
 */
#include <kernel.h>
#include <init.h>
#include "os_common_api.h"
#include "global_mem.h"

#ifdef CONFIG_USER_WORK_Q
os_work_q user_work_q;

/** user work queue tread */
char __noinit  __aligned(STACK_ALIGN) user_work_q_stack[USER_WQ_STACK_SIZE];

static int user_work_q_init(struct device *dev)
{
	ARG_UNUSED(dev);

	os_work_q_start(&user_work_q,
		       (os_thread_stack_t)user_work_q_stack,
		       sizeof(user_work_q_stack),
		       CONFIG_USER_WORK_Q_PRIORITY);

	return 0;
}

SYS_INIT(user_work_q_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
#endif

os_work_q *get_user_work_queue(void)
{
#ifdef CONFIG_USER_WORK_Q
	return &user_work_q;
#else
	return NULL;
#endif
}