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

#include "os_common_api.h"
#include "msg_manager.h"
#include "string.h"
#include "stack_backtrace.h"
#include <kernel.h>
#include <ksched.h>

#include <logging/sys_log.h>

/**thread function */
int os_thread_create(char *stack, size_t stack_size,
					 void (*entry)(void *, void *, void*),
					 void *p1, void *p2, void *p3,
					 int prio, u32_t options, s32_t delay) {
	int tid;
	os_thread * thread = NULL;

	thread = (os_thread *)stack;

	tid = (int)k_thread_create(thread, (os_thread_stack_t)&stack[sizeof(os_thread)],
							stack_size - sizeof(os_thread),
							entry,
							p1, p2, p3,
							prio,
							options,
							delay);

	return tid;
}

/**message function*/

K_MBOX_DEFINE(global_mailbox);

/** message pool */
struct msg_info
{
	os_sem msg_sem;
#ifdef CONFIG_MESSAGE_DEBUG
	char *sender;
	char *receiver;
#endif
	struct app_msg msg;
};

struct msg_pool
{
	int pool_size;
	struct msg_info *pool;
};

__in_section_unique(MBOX_MSGS_BSS) static struct msg_info msg_pool_buff[CONFIG_NUM_MBOX_ASYNC_MSGS];

static struct msg_pool globle_msg_pool= {
	.pool_size = CONFIG_NUM_MBOX_ASYNC_MSGS,
	.pool = (struct msg_info *)&msg_pool_buff,
};
static os_msg_match globle_msg_match;

static void msg_pool_info_dump(struct msg_pool *pool)
{
#ifdef CONFIG_MESSAGE_DEBUG

	extern u32_t mem_is_pool_data(u32_t addr);

	struct msg_info *msg_content;
	struct app_msg *msg;

	for (u8_t i = 0; i < pool->pool_size; i++) {
		msg_content = &pool->pool[i];
		if (k_sem_take(&msg_content->msg_sem, OS_NO_WAIT) == 0) {
			k_sem_give(&msg_content->msg_sem);
		} else {
			msg = &msg_content->msg;
			printk("msg: %d\n", i);
			printk("--sender %s\n", msg_content->sender);
			printk("--receiver %s\n", msg_content->receiver);
			printk("--type 0x%x\n", msg->type);
			printk("--cmd 0x%x\n", msg->cmd);
			printk("--content 0x%x\n\n", msg->value);
			if(mem_is_pool_data((u32_t)msg->ptr)){
				print_buffer(msg->ptr, 1, 16, 16, -1);
			}
		}
	}
#endif

}

static struct msg_info *msg_pool_get_free_msg_info(void)
{
	struct msg_pool *pool = &globle_msg_pool;
	struct msg_info *result = NULL;

	for (u8_t i = 0 ; i < pool->pool_size; i++) {
		struct msg_info * msg_content = &pool->pool[i];
		if (k_sem_take(&msg_content->msg_sem, OS_NO_WAIT) == 0) {
			memset(&msg_content->msg, 0, sizeof(struct app_msg));
			result = msg_content;
			break;
		}
	}

#if 0
#ifdef CONFIG_MESSAGE_DEBUG
	static int flag = 0;
	if (msg_pool_get_free_msg_num() < (CONFIG_NUM_MBOX_ASYNC_MSGS / 5)) {
		if (flag++ != 0) {
			show_all_threads_stack();
		}
	}
#endif
#endif

	if (NULL == result) {
		msg_pool_info_dump(pool);
		SYS_LOG_WRN("msg is full %d\n", CONFIG_NUM_MBOX_ASYNC_MSGS);
	}
	return result;
}

int msg_pool_get_free_msg_num(void)
{
	struct msg_pool *pool = &globle_msg_pool;
	int used_num = 0;
	SYS_IRQ_FLAGS key;

	sys_irq_lock(&key);

	for (u8_t i = 0 ; i < pool->pool_size; i++) {
		struct msg_info * msg_content = &pool->pool[i];

		if (os_sem_take(&msg_content->msg_sem, OS_NO_WAIT) != 0) {
			used_num++;
		} else {
			os_sem_give(&msg_content->msg_sem);
		}
	}

	sys_irq_unlock(&key);

	return pool->pool_size - used_num;
}

static void os_sync_msg_callback(struct app_msg* msg, int result, void* not_used)
{
	if (msg->sync_sem) {
#ifdef CONFIG_MESSAGE_DEBUG
		printk("--(%s->%s)-- %s %d: type_id %d, msg_id %d, e_time %u\n",
				"",
				msg_manager_get_current(),
				__func__, __LINE__,
				msg->type,
				msg->cmd,
				k_uptime_get_32());
#endif

		os_sem_give(msg->sync_sem);
	}
}

int os_send_sync_msg(void *receiver, void *msg, int msg_size)
{
	os_mbox_msg send_msg;
	struct app_msg msg_content;
	struct k_sem sync_sem;
	struct app_msg *tmp_msg = NULL;

	__ASSERT(!_is_in_isr(),"send messag in isr");

	memcpy(&msg_content, msg, msg_size);

	tmp_msg = &msg_content;
    tmp_msg->callback = os_sync_msg_callback;
    tmp_msg->sync_sem = &sync_sem;
    k_sem_init(&sync_sem, 0, UINT_MAX);

    /* prepare to send message */
    send_msg.info = sizeof(struct app_msg);
    send_msg.size = sizeof(struct app_msg);
    send_msg.tx_data = &msg_content;
    send_msg.tx_target_thread = (os_tid_t)receiver;

    /* send message containing most current data and loop around */
    os_mbox_put(&global_mailbox, &send_msg, OS_FOREVER);

    os_sem_take(&sync_sem, OS_FOREVER);

	return 0;
}

int os_send_async_msg(void *receiver, void *msg, int msg_size)
{
	os_mbox_msg send_msg;
	struct msg_info *msg_content;

	__ASSERT(!_is_in_isr(),"send messag in isr");

	msg_content = msg_pool_get_free_msg_info();

	if(!msg_content) {
		SYS_LOG_ERR("msg_content is NULL ... ");
		return -ENOMEM;
	}

	memcpy(&msg_content->msg, msg, msg_size);
#ifdef CONFIG_MESSAGE_DEBUG
	msg_content->receiver = msg_manager_get_name_by_tid((int)receiver);
	msg_content->sender = msg_manager_get_name_by_tid((int)os_current_get());
	if(msg_content->sender == NULL)
	{
		msg_content->sender = (char *)os_current_get();
	}
#endif
    /* prepare to send message */
    send_msg.info = sizeof(struct app_msg);
    send_msg.size = sizeof(struct app_msg);
    send_msg.tx_data = &msg_content->msg;
    send_msg.tx_target_thread = (os_tid_t)receiver;

    /* send message containing most current data and loop around */
    os_mbox_async_put(&global_mailbox, &send_msg, &msg_content->msg_sem);

	return 0;
}

int os_receive_msg(void *msg, int msg_size,int timeout)
{
	os_mbox_msg recv_msg;
    char buffer[sizeof(struct app_msg)];

	memset(buffer, 0, sizeof(struct app_msg));

	/* prepare to receive message */
	recv_msg.info =  sizeof(struct app_msg);
	recv_msg.size =  sizeof(struct app_msg);
	recv_msg.rx_source_thread = OS_ANY;

    /* get a data item, waiting as long as needed */
	if(os_mbox_get(&global_mailbox, &recv_msg, buffer, timeout))
	{
		//SYS_LOG_INF("no message");
		return -ETIMEDOUT;
	}

	/* verify that message data was fully received */
	if (recv_msg.info != recv_msg.size) {
	    SYS_LOG_ERR("some message data dropped during transfer! \n ");
	    SYS_LOG_ERR("sender tried to send %d bytes"
					"only received %zu bytes receiver %p \n",
					recv_msg.info ,recv_msg.size,os_current_get());
	    return -EMSGSIZE;
	}

	/* copy msg from recvied buffer */
	memcpy(msg, buffer, msg_size);

	return 0;
}

void os_msg_clean(void)
{
	os_mbox_clear_msg(&global_mailbox);
}

int os_get_pending_msg_cnt(void)
{
	return k_mbox_get_pending_msg_cnt(&global_mailbox, os_current_get());
}

void os_msg_init(void)
{
	struct msg_pool *pool = &globle_msg_pool;
	for (u8_t i = 0 ; i < pool->pool_size; i++) {
		struct msg_info *msg_content = &pool->pool[i];
		os_sem_init(&msg_content->msg_sem, 1, 1);
	}
}

static int _os_msg_match(struct k_mbox_msg *msg){
	if(!msg || !globle_msg_match)
		return 0;
	struct app_msg *tmp_msg = (struct app_msg *)(msg->tx_data);
	if(!tmp_msg)
		return 0;
	if(globle_msg_match(tmp_msg,msg->tx_target_thread,msg->rx_source_thread)){
		return 1;
	}
	return 0;
}

int os_msg_delete(os_msg_match msg_match)
{
	os_sched_lock();
	globle_msg_match = msg_match;
	int ret = os_mbox_delete_msg(&global_mailbox,_os_msg_match);
	globle_msg_match = NULL;
	os_sched_unlock();
	return ret;
}

static bool low_latency_mode = true;

int system_check_low_latencey_mode(void)
{
#ifdef CONFIG_OS_LOW_LATENCY_MODE
	return low_latency_mode ? 1 : 0;
#else
	return 0;
#endif
}

void system_set_low_latencey_mode(bool low_latencey)
{
	low_latency_mode = low_latencey;
}

bool os_is_in_isr(void)
{
	return k_is_in_isr();
}

int os_is_free_msg_enough(void)
{
	if(msg_pool_get_free_msg_num() > 5){
        return true;
	}
	else{
		//msg_pool_info_dump(&globle_msg_pool);
		return false;
	}
}

