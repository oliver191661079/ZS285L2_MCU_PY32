
/*!
 * \file
 * \brief
 * \details
 * \author    Tommy Wang
 * \date
 * \copyright Actions
 */

#ifndef _MP_APP_H
#define _MP_APP_H


#include "mp_data.h"

#define MP_DEFAULT_CHANNEL    39

typedef void (*mp_app_callback_func)(uint_t event, void *param);

typedef enum
{
	THREAD_NULL,
	THREAD_CREATE,
	THREAD_RUNNING,
	THREAD_REQUEST_EXIT,
	THREAD_EXIT,
} thread_status_e;

enum
{
    K_MP_PN9 =            0,
    K_MP_PN15 =           1,
    K_MP_ALL_0 =          2 ,
    K_MP_ALL_1  =         3 ,
    K_MP_00001111 =       4,
    K_MP_01010101 =       5,
};

enum
{
    MP_PKT_ID     = 0,
    MP_PKT_POLL   = 1,
    MP_PKT_FHS    = 2,
    MP_PKT_DM1    = 3,
    MP_PKT_DH1    = 4,
    MP_PKT_HV1    = 5,
    MP_PKT_HV2    = 6,
    MP_PKT_HV3    = 7,
    MP_PKT_DV     = 8,
    MP_PKT_AUX1   = 9,
    MP_PKT_DM3    = 10,
    MP_PKT_DH3    = 11,
    MP_PKT_EV4    = 12,
    MP_PKT_EV5    = 13,
    MP_PKT_DM5    = 14,
    MP_PKT_DH5    = 15,
};


typedef struct
{
    char  high_prio_thread_stack[2048];
    mp_manager_info_t manager_info;
    char  high_prio_thread_schedule[512];
}mp_private_t;

//extern void *mp_bt_control_handle(void *param);

extern void mp_system_hardware_init(void);

extern void mp_system_hardware_deinit(void);

extern void mp_core_init(mp_manager_info_t * mp_manager);

extern int8 mp_set_packet_param(mp_packet_ctrl_t * packet_ctrl);

extern uint8 mp_packet_send_process(void);

extern uint8 mp_packet_receive_process(void);

extern uint8 mp_single_tone_process(mp_manager_info_t * mp_manager);

extern uint32 mp_get_hosc_cap(void);

extern void mp_set_hosc_cap(uint32 cap);

extern void mp_controller_request_irq(void);

extern void mp_controller_free_irq(void);

extern int i2capp_init(void);

#endif  // _MP_APP_H

