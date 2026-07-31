/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file massage manager interface
 */

#ifndef __MSG_MANAGER_H__
#define __MSG_MANAGER_H__

#include <os_common_api.h>

#define MSG_CONTENT_SIZE 4

typedef enum
{
	MSG_NULL = 0,

	MSG_START_APP ,
	MSG_SWITCH_APP,
	MSG_INIT_APP,
	MSG_EXIT_APP,
	MSG_SUSPEND_APP,
	MSG_RESUME_APP,

	MSG_KEY_INPUT,
	MSG_INPUT_EVENT,
	MSG_UI_EVENT,
	MSG_TTS_EVENT = 10,
	MSG_HOTPLUG_EVENT,
	MSG_VOLUME_CHANGED_EVENT,

	MSG_POWER_OFF,
	MSG_STANDBY,
	MSG_LOW_POWER,
	MSG_NO_POWER,
	MSG_REBOOT,

    /* sync message no replay */
	MSG_REPLY_NO_REPLY,

	/* message process success */
	MSG_REPLY_SUCCESS,

	/* message process failed */
	MSG_REPLY_FAILED = 20,

	MSG_BAT_CHARGE_EVENT,
	MSG_SR_INPUT,
	
	MSG_WIFI_EVENT,
	MSG_RETURN_TO_CALLER,

	MSG_BT_EVENT,

	MSG_BT_ENGINE_READY,
	MSG_BT_MGR_EVENT ,

	MSG_TWS_EVENT ,

	MSG_SHUT_DOWN,

	MSG_BT_REMOTE_VOL_SYNC = 30,

	MSG_SYS_EVENT,

	MSG_AUTOTEST_START_BT,

	MSG_CHARGER_MODE,

	MSG_SWITCH_APP_LOCK_EVENT,
	/* service defined message */
	MSG_SRV_MESSAGE_START           = 128,

	MSG_BTSRV_MESSAGE_START         = 160,

	/* application defined message */
	MSG_APP_MESSAGE_START           = 200,
} msg_type;


struct msg_listener
{
	sys_snode_t node;
	char * name;
	k_tid_t tid;
};

#define LISTENER_INFO(_node) CONTAINER_OF(_node, struct msg_listener, node)



/**
 * @defgroup msg_manager_apis Message Manager APIs
 * @ingroup system_apis
 * @{
 */

struct app_msg;

typedef void (*MSG_CALLBAK)(struct app_msg*, int, void*);

#define send_async_msg(receiver, msg) \
			msg_manager_send_async_msg(receiver, msg)

#define receive_msg(msg, timeout) \
			msg_manager_receive_msg(msg, timeout)

/** message structure */
/** @brief app message structure.
 *  @param sender who send this message
 *  @param type message type ,keyword of message
 *  @param cmd message cmd , used to send opration type
 *  @param reserve  resrved byte, user can used this byte set info
 *  @param valuuser data , to transmission some user info
 *  @param callback callback of message process finished.
 *  @param sync_sem sem for sync.
 */
struct app_msg
{
	uint8_t sender;
	uint8_t type;
	uint8_t cmd;
	uint8_t reserve;
	union {
		char content[MSG_CONTENT_SIZE];
		short short_content[MSG_CONTENT_SIZE / 2];
		int value;
		void *ptr;
	};
	MSG_CALLBAK callback;
	struct k_sem *sync_sem;
};

/**
 * @brief msg_manager_init
 *
 * This routine msg_manager_init
 *
 * @return true  init success
 * @return false init failed
 */

bool msg_manager_init(void);

/**
 * @brief Send a Asynchronous message
 *
 * This routine Send a Asynchronous message
 *
 * @param receiver name of message receiver
 * @param msg store the received message
 *
 * @return true send success
 * @return false send failed
 */
bool msg_manager_send_async_msg(char * receiver , struct app_msg *msg);

/**
 * @brief receive message
 *
 * This routine receive msg from others
 *
 * @param msg store the received message
 * @param timeout time out of receive message
 *
 * @return NULL The listener is not in the message linsener list
 * @return tid  target thread id of the message linsener
 */
bool msg_manager_receive_msg(struct app_msg *msg, int timeout);
/**
 * @brief get free async msg num
 *
 * Asynchronous message has a number of restrictions, when sending an asynchronous message,
 * the recipient did not receive before, asynchronous message occupies a memory,
 * only when the recipient copy will be released after the release of the memory,
 * the current support of up to 10 asynchronous messages,
 * This routine Returns the number of asynchronous free asynchronous messages
 *
 * @return NUM  num of async msg num
 */
int msg_manager_get_free_msg_num(void);

/**
 * @brief drop all message
 *
 * This routine drop all message in mail box
 *
 */

void msg_manager_drop_all_msg(void);

/**
 * @brief check have new message
 *
 * This routine check have new in mail box
 *
 * @return number of new message in mailbox
 */
int msg_manager_get_pending_msg_cnt(void);

/**
 * @cond INTERNAL_HIDDEN
 */
/**
 * @brief Send a synchronization message
 *
 * This routine Send a synchronization message
 *
 * @param receiver name of message receiver
 * @param msg store the received message
 *
 * @return true send success
 * @return false send failed
 */
#define send_sync_msg(receiver, msg) \
			msg_manager_receive_msg(msreceiverg, msg)
bool msg_manager_send_sync_msg(char *receiver , struct app_msg *msg);
/**
 * @brief add message listener
 *
 * This routine add target thread to messsage lisener list
 * all app or service who want to receive message from others, must
 * add themslef to message listener
 *
 * @param name target thread name ,witch marked thread.
 * @param tid target thread id.
 *
 * @return true execution succeed
 * @return false execution failed
 */
bool msg_manager_add_listener(char * name , k_tid_t tid);

/**
 * @brief remove listener from message listener list
 *
 * This routine remove target thread from messsage lisener list
 * all app or service must removed from message listener list
 * before is will exit.
 *
 * @param name target thread name ,witch marked thread.
 *
 * @return true execution succeed
 * @return false execution failed
 */

bool msg_manager_remove_listener(char * name);

/**
 * @brief get the thread id from message listener list by name
 *
 * This routine remove target thread from messsage lisener list
 * all app or service must removed from message listener list
 * before is will exit.
 *
 * @param name target thread name ,witch marked thread.
 *
 * @return NULL The listener is not in the message linsener list
 * @return tid  target thread id of the message linsener
 */

k_tid_t  msg_manager_listener_tid(char * name);
/**
 * @brief get receiver name by tid
 *
 * This routine get receiver name by tid
 *
 * @param tid thread id
 *
 * @return name of receiver for target tid
 */
char* msg_manager_get_name_by_tid(int tid);

/**
 * @brief get current sender name
 *
 * This routine get current sender name
 *
 * @return name of sender
 */
char* msg_manager_get_current(void);

/**
 * @brief lock msg manager
 *
 * This routine lock msg manager
 *
 * @return N/A
 */
void msg_manager_lock(void);

/**
 * @brief unlock msg manager
 *
 * This routine gunlock msg manager
 *
 * @return N/A
 */

void msg_manager_unlock(void);

/* message to string
 */
typedef struct
{
    uint8_t     msg;
    const char* str;

} __packed msg2str_map_t;

const char* msg2str(uint8_t msg, const msg2str_map_t* maps,
    int num, const char* unmapped);

const char* sys_msg2str(uint8_t msg, const char* unmapped);

/**
 * INTERNAL_HIDDEN @endcond
 */

/**
 * @} end defgroup msg_manager_apis
 */


#endif
