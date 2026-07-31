/*
 * Copyright (c) 1997-2015, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <misc/util.h>
#include <logging/sys_log.h>
#include <mem_manager.h>
#include "dsp_inner.h"

extern const struct dsp_imageinfo *dsp_create_image(const char *name);
extern void dsp_free_image(const struct dsp_imageinfo *image);

static struct dsp_session *global_session = NULL;
static unsigned int global_uuid = 0;



int dsp_session_get_state(struct dsp_session *session)
{
	int state = 0;
	dsp_request_userinfo(session->dev, DSP_REQUEST_TASK_STATE, &state);
	return state;
}

int dsp_session_get_error(struct dsp_session *session)
{
	int error = 0;
	dsp_request_userinfo(session->dev, DSP_REQUEST_ERROR_CODE, &error);
	return error;
}

uint32_t dsp_session_get_debug_info(struct dsp_session *session, uint32_t index)
{
	dsp_request_userinfo(session->dev, DSP_REQUEST_USER_DEFINED, &index);
	return index;
}

uint32_t dsp_session_get_func_counter(struct dsp_session *session)
{
	struct dsp_request_session request;
	if (!session) {
		/* implicitly point to global session */
		session = global_session;
		if (!session)
			return 0;
	}
	dsp_request_userinfo(session->dev, DSP_REQUEST_SESSION_INFO, &request);
	return request.func_counter;
}

void dsp_session_dump(struct dsp_session *session)
{
	struct dsp_request_session request;

	if (!session) {
		/* implicitly point to global session */
		session = global_session;
		if (!session)
			return;
	}

	dsp_request_userinfo(session->dev, DSP_REQUEST_SESSION_INFO, &request);

	printk("\n---------------------------dsp dump---------------------------\n");
	printk("session (id=%u, uuid=%u):\n", session->id, session->uuid);
	printk("\tstate=%d\n", dsp_session_get_state(session));
	printk("\terror=0x%x\n", dsp_session_get_error(session));
	printk("\tfunc_allowed=0x%x\n", session->func_allowed);
	printk("\tfunc_enabled=0x%x\n", request.func_enabled);
	printk("\tfunc_runnable=0x%x\n", request.func_runnable);
	printk("\tfunc_counter=0x%x\n", request.func_counter);

	if (request.info)
		dsp_session_dump_info(session, request.info);
	if (request.func_enabled) {
		for (int i = 0; i < DSP_NUM_FUNCTIONS; i++) {
			if (request.func_enabled & DSP_FUNC_BIT(i))
				dsp_session_dump_function(session, i);
		}
	}
	printk("--------------------------------------------------------------\n\n");
}

static int dsp_session_set_id(struct dsp_session *session)
{
	struct dsp_session_id_set set = {
		.id = session->id,
		.uuid = session->uuid,
		.func_allowed = session->func_allowed,
	};

	struct dsp_command *command =
		dsp_command_alloc(DSP_CMD_SET_SESSION, NULL, sizeof(set), &set);
	if (command == NULL)
		return -ENOMEM;

	dsp_session_submit_command(session, command);
	dsp_command_free(command);
	return 0;
}

static inline int dsp_session_get(struct dsp_session *session)
{
	return atomic_inc(&session->open_refcnt);
}

static inline int dsp_session_put(struct dsp_session *session)
{
	return atomic_dec(&session->open_refcnt);
}

static void dsp_session_destroy(struct dsp_session *session);
static struct dsp_session *dsp_session_create(struct dsp_session_info *info)
{
	struct dsp_session *session = NULL;
	struct acts_ringbuf *ringbuf = NULL;

	if (info == NULL || info->main_dsp == NULL)
		return NULL;

	session = mem_malloc(sizeof(*session));
	if (session == NULL)
		return NULL;

	memset(session, 0, sizeof(*session));

	/* find dsp device */
	session->dev = device_get_binding(CONFIG_DSP_ACTS_DEV_NAME);
	if (session->dev == NULL) {
		SYS_LOG_ERR("cannot find device \"%s\"", CONFIG_DSP_ACTS_DEV_NAME);
		goto fail_exit;
	}

	/* find images */
	session->images[DSP_IMAGE_MAIN] = dsp_create_image(info->main_dsp);
	if (!session->images[DSP_IMAGE_MAIN]) {
		goto fail_exit;
	}
#ifdef CONFIG_MUSIC_EFFECT
	if (info->sub_dsp) {
		session->images[DSP_IMAGE_SUB] = dsp_create_image(info->sub_dsp);
		if (!session->images[DSP_IMAGE_SUB]) {
			goto fail_exit;
		}
	}
#endif
	/* allocate command buffer */
	ringbuf = acts_ringbuf_alloc(DSP_SESSION_COMMAND_BUFFER_SIZE);
	if (ringbuf == NULL) {
		SYS_LOG_ERR("malloc failed");
		goto fail_exit;
	}

	session->cmdbuf.cur_seq = 0;
	session->cmdbuf.alloc_seq = 1;
	session->cmdbuf.cpu_bufptr = (uint32_t)ringbuf;
	session->cmdbuf.dsp_bufptr = mcu_to_dsp_data_address(session->cmdbuf.cpu_bufptr);

	k_sem_init(&session->sem, 0, 1);
	k_mutex_init(&session->mutex);
	atomic_set(&session->open_refcnt, 0);
#ifdef ENABLE_SESSION_BEGIN_END_REFCNT
	atomic_set(&session->run_refcnt, 0);
#endif
	session->id = info->type;
	session->uuid = ++global_uuid;
	SYS_LOG_INF("created (id=%u, uuid=%u)", session->id, session->uuid);
	return session;
fail_exit:
	dsp_session_destroy(session);
	return NULL;
}

static void dsp_session_destroy(struct dsp_session *session)
{
	if (atomic_get(&session->open_refcnt) != 0) {
		SYS_LOG_ERR("open_refcnt=%d", atomic_get(&session->open_refcnt));
	}

	for (int i = 0; i < ARRAY_SIZE(session->images); i++) {
		if (session->images[i])
			dsp_free_image(session->images[i]);
	}

	if (session->cmdbuf.cpu_bufptr)
		acts_ringbuf_free((struct acts_ringbuf *)(session->cmdbuf.cpu_bufptr));

	SYS_LOG_INF("destroyed (id=%u, uuid=%u)", session->id, session->uuid);
	mem_free(session);
}

static int dsp_session_message_handler(struct dsp_message *message)
{
	if (!global_session || message->id != DSP_MSG_KICK)
		return -ENOTTY;

	switch (message->param1) {
	case DSP_EVENT_FENCE_SYNC:
		if (message->param2){
			if(k_sem_count_get((struct k_sem *)message->param2) > 1){
				SYS_LOG_ERR("invalid %p", (struct k_sem *)message->param2);
			}else{
				k_sem_give((struct k_sem *)message->param2);
			}
		}
		break;
	case DSP_EVENT_NEW_CMD:
	case DSP_EVENT_NEW_DATA:
		k_sem_give(&global_session->sem);
		break;
	// case 13:
	// 	printk("--%d-%d--\n", message->param1, message->param2);
	// 	break;
	default:
		return -ENOTTY;
	}

	return 0;
}

static int dsp_session_open(struct dsp_session *session, struct dsp_session_info *info)
{
	int res = 0;

	if (dsp_session_get(session) != 0) {
		SYS_LOG_WRN("opened already uuid=%u", session->uuid);
		return -EALREADY;
	}

	if (info == NULL) {
		res = -EINVAL;
		goto fail_out;
	}

	/* load main dsp image */
	res = dsp_request_image(session->dev, session->images[DSP_IMAGE_MAIN], DSP_IMAGE_MAIN);
	if (res) {
		SYS_LOG_ERR("failed to load main image \"%s\"", session->images[DSP_IMAGE_MAIN]->name);
		goto fail_out;
	}

	/* load sub dsp image */
	if (session->images[DSP_IMAGE_SUB]) {
		res = dsp_request_image(session->dev, session->images[DSP_IMAGE_SUB], DSP_IMAGE_SUB);
		if (res) {
			SYS_LOG_ERR("failed to load sub image \"%s\"", session->images[DSP_IMAGE_SUB]->name);
			goto fail_release_main;
		}
	}

	/* reset and register command buffer */
	session->cmdbuf.cur_seq = 0;
	session->cmdbuf.alloc_seq = 1;
	acts_ringbuf_reset((struct acts_ringbuf *)(session->cmdbuf.cpu_bufptr));

	/* power on */
	res = dsp_poweron(session->dev, &session->cmdbuf);
	if (res) {
		SYS_LOG_ERR("dsp_poweron failed");
		goto fail_release_sub;
	}

	/* configure session id to dsp */
	session->func_allowed = info->func_allowed;
	res = dsp_session_set_id(session);
	if (res) {
		SYS_LOG_ERR("dsp_session_set_id failed");
		dsp_poweroff(session->dev);
		goto fail_release_sub;
	}

	/* register message handler only after everything OK */
	k_sem_reset(&session->sem);
	dsp_register_message_handler(session->dev, dsp_session_message_handler);
	SYS_LOG_INF("session %u opened (uuid=%u, allowed=0x%x)",
			session->id, session->uuid, info->func_allowed);
	return 0;
fail_release_sub:
	if (session->images[DSP_IMAGE_SUB])
		dsp_release_image(session->dev, DSP_IMAGE_SUB);
fail_release_main:
	dsp_release_image(session->dev, DSP_IMAGE_MAIN);
fail_out:
	dsp_session_put(session);
	return res;
}

static int dsp_session_close(struct dsp_session *session)
{
	if (dsp_session_put(session) != 1)
		return -EBUSY;

	if (session->cmdbuf.cur_seq + 1 != session->cmdbuf.alloc_seq){
		printk("[ERR] session close err:%d %d\n", session->cmdbuf.alloc_seq, session->cmdbuf.cur_seq);
	}

	dsp_unregister_message_handler(session->dev);
	dsp_poweroff(session->dev);

	for (int i = 0; i < ARRAY_SIZE(session->images); i++) {
		if (session->images[i])
			dsp_release_image(session->dev, i);
	}

	SYS_LOG_INF("session %u closed (uuid=%u)", session->id, session->uuid);
	return 0;
}

struct dsp_session *dsp_open_global_session(struct dsp_session_info *info)
{
	int res;

	if (global_session == NULL) {
		if (info) {
			global_session = dsp_session_create(info);
			if (global_session == NULL) {
				SYS_LOG_ERR("failed create");
				return NULL;
			}
		} else {
			return NULL;
		}
	}

	if (info) {
		res = dsp_session_open(global_session, info);
		if (res < 0 && res != -EALREADY) {
			SYS_LOG_ERR("failed open");
			dsp_session_destroy(global_session);
			global_session = NULL;
		}
	} else {
		res = dsp_session_open(global_session, info);
		if (res < 0 && res != -EALREADY) {
			SYS_LOG_ERR("failed open");
			return NULL;
		}
	}

	return global_session;
}
struct dsp_session * dsp_get_global_session(void)
{
	return global_session;
}

void dsp_close_global_session(struct dsp_session *session)
{
	//assert(session == global_session);

	if (global_session) {
		if (!dsp_session_close(global_session)) {
			dsp_session_destroy(global_session);
			global_session = NULL;
		}
	}
}

int dsp_session_wait(struct dsp_session *session, int timeout)
{
	return k_sem_take(&session->sem, timeout);
}

int dsp_session_kick(struct dsp_session *session)
{
	if(session == NULL){
		if(global_session){
			session = global_session;
			return dsp_kick(session->dev, session->uuid, DSP_EVENT_NEW_DATA, 0);
		}else{
			return 0;
		}
	}else{
		return dsp_kick(session->dev, session->uuid, DSP_EVENT_NEW_DATA, 0);
	}
}

int dsp_session_function_trigger(int function_id)
{
	if (get_function_trigger(function_id)) {
		//printk("<dsp>%d not clear\n", function_id);
	} else {
		set_function_trigger(function_id);
		if(global_session){
			return dsp_kick(global_session->dev, global_session->uuid, DSP_EVENT_TRIGGER, function_id);
		}
	}
	return 0;
}


int dsp_session_resume(struct dsp_session *session)
{
	if(session == NULL){
		if(global_session){
			session = global_session;
			return dsp_resume(session->dev);
		}else{
			return 0;
		}
	}else{
		return dsp_resume(session->dev);
	}
}


int dsp_session_submit_command(struct dsp_session *session, struct dsp_command *command)
{
	struct acts_ringbuf *buf = (struct acts_ringbuf *)(session->cmdbuf.cpu_bufptr);
	unsigned int size = sizeof_dsp_command(command);
	unsigned int space;
	unsigned int wait_time = 500;
	int res = -ENOMEM;
	int irq_flag;
	u64_t prev_head;

	if (!global_session){
        return -ENODEV;
	}

	k_mutex_lock(&session->mutex, K_FOREVER);

#ifdef ENABLE_SESSION_BEGIN_END_REFCNT
	if ((command->id == DSP_CMD_SESSION_BEGIN && atomic_inc(&session->run_refcnt) != 0) ||
		(command->id == DSP_CMD_SESSION_END && atomic_dec(&session->run_refcnt) != 1)) {
		if (command->sem) {
			SYS_LOG_DBG("ignore command %d", command->id);
			k_sem_give((struct k_sem *)command->sem);
		}

		res = 0;
		goto out_unlock;
	}
#endif

    while(wait_time){
        space = acts_ringbuf_space(buf);
        if (space < ACTS_RINGBUF_NELEM(size)) {
            k_sleep(10);
            wait_time -= 10;
			SYS_LOG_ERR("wait cmd space %d %d %d\n",  size, acts_ringbuf_head_offset(buf), acts_ringbuf_tail_offset(buf));
		}else{
            break;
        }
    }

    if (wait_time == 0){
        SYS_LOG_ERR("No enough space (%u) for command (id=0x%04x, size=%u)",
                space, command->id, size);

#ifdef ENABLE_SESSION_BEGIN_END_REFCNT
		switch (command->id) {
		case DSP_CMD_SESSION_BEGIN:
			atomic_dec(&session->run_refcnt);
			break;
		case DSP_CMD_SESSION_END:
			atomic_inc(&session->run_refcnt);
			break;
		default:
			break;
		}
#endif
        goto out_unlock;
    }

	//SYS_LOG_INF("command %d data %d %d", command->id, command->data[0], command->data[1]);
	irq_flag = irq_lock();

	/* alloc sequence number */
	command->seq = session->cmdbuf.alloc_seq++;

	prev_head = buf->head;

	/* insert command */
	res = acts_ringbuf_put(buf, command, ACTS_RINGBUF_NELEM(size));

	irq_unlock(irq_flag);

	wait_time = 0;
	while(buf->head == prev_head){
		k_sleep(1);
		wait_time++;
		if(wait_time == 10){
			SYS_LOG_ERR("cmd seq %d id %d head %d tail %d timeout\n", command->seq, command->id, (uint32_t)buf->head, (uint32_t)buf->tail);
			break;
		}
	}

	//os_sleep(5);

	//dsp_session_dump(NULL);

	if(res == 0){
		panic("send command error\n");
	}

#ifdef CONFIG_DSP_SUSPEND
	res = dsp_resume(session->dev);
#endif

#ifdef CONFIG_DSP_LIGHT_SLEEP
	/* kick dsp to process command */
	res = dsp_kick(session->dev, session->uuid, DSP_EVENT_NEW_CMD, 0);
#endif

	res = 0;
out_unlock:
	k_mutex_unlock(&session->mutex);
	return res;
}


