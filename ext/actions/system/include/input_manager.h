/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file input manager interface 
 */

#ifndef __INPUT_MANGER_H__
#define __INPUT_MANGER_H__
#include <input_dev.h>
/**
 * @defgroup input_manager_apis App Input Manager APIs
 * @ingroup system_apis
 * @{
 */

/** key action type */
enum KEY_VALUE
{
	/** key press down */
    KEY_VALUE_DOWN      = 1,
	/** key press release */
	KEY_VALUE_UP       = 0,
};

/** key event STAGE */
enum KEY_EVENT_STAGE
{
	/** key press pre process */
    KEY_EVENT_PRE_DONE   = 0,
	/** key press event done */
	KEY_EVENT_DONE       = 1,
};

/** key press type */
typedef enum
{
    /** the max value of key type, it must be less than  MASK_KEY_UP*/
    KEY_TYPE_NULL       	= 0x00000000,
    KEY_TYPE_LONG_DOWN  	= 0x10000000,
    KEY_TYPE_SHORT_DOWN 	= 0x20000000,
    KEY_TYPE_DOUBLE_CLICK 	= 0x40000000,
    KEY_TYPE_HOLD       	= 0x08000000,
    KEY_TYPE_SHORT_UP   	= 0x04000000,
    KEY_TYPE_LONG_UP    	= 0x02000000,
    KEY_TYPE_HOLD_UP    	= 0x01000000,
    KEY_TYPE_LONG6S_UP 		= 0x00800000,
    KEY_TYPE_LONG6S      	= 0x00400000, 
	KEY_TYPE_LONG      		= 0x00200000, 
    KEY_TYPE_TRIPLE_CLICK 	= 0x00100000,
    KEY_TYPE_ALL        	= 0xFFF00000,
}key_type_e;

typedef void (*event_trigger)(uint32_t key_value, uint16_t type);

/**
 * @brief input manager init funcion
 *
 * This routine calls init input manager ,called by main
 *
 * @param event_cb when keyevent report ,this callback called before
 * key event dispatch.
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */

bool input_manager_init(event_trigger event_cb);

/**
 * @brief Lock the input event
 *
 * This routine calls lock input event, input event may not send to system
 * if this function called.
 *
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */

bool input_manager_lock(void);

/**
 * @brief unLock the input event
 *
 * This routine calls unlock input event, input event may send to system
 * if this function called.
 *
 * @note input_manager_lock and input_manager_unlock Must match
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */

bool input_manager_unlock(void);
/**
 * @brief get the status of input event lock
 *
 * @return true if input event is locked
 * @return false if input event is not locked.
 */
bool input_manager_islock(void);

/**
 * @brief set filter key event
 *
 * @return true set success
 * @return false set failed.
 */
bool input_manager_filter_key_itself(void);
/**
 * @} end defgroup input_manager_apis
 */
#endif