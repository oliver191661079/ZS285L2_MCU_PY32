#ifndef __SYSTEM_APP_H
#define __SYSTEM_APP_H

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "sysapp"
#ifndef SYS_LOG_LEVEL
#undef SYS_LOG_LEVEL
#endif
#include <logging/sys_log.h>

typedef struct
{
    u8_t in_front_charge;
    u8_t in_test_mode;
    u8_t in_power_off_stage;
    u8_t in_power_off_charge;
    u8_t bat_is_too_low;
    u8_t bat_too_low_powoff;
    u8_t charge_state;
    u8_t charger_box_bat_level;
    u8_t charger_box_opened;
    u8_t in_charger_box;
    u8_t audio_media_mutex_stopped;
    u8_t in_ota_update;
    u8_t tws_remote_in_charger_box;
    u8_t chg_box_bat_low;

    u8_t disable_auto_standby;
    u8_t disable_front_charge;
    u8_t disable_key_tone;
    u8_t disable_audio_tone;
    u8_t disable_audio_voice;

    u16_t reboot_type;
    u8_t reboot_reason;
    u8_t anc_mode;
} system_status_t;

typedef struct
{
    system_status_t sys_status;

#ifdef CONFIG_TWS_UI_EVENT_SYNC
    struct list_head notify_ctrl_list;
    struct thread_timer notify_ctrl_timer;
#endif

} system_app_context_t;

extern system_app_context_t system_app_context;
static inline system_app_context_t *system_app_get_context(void)
{
    return &system_app_context;
}

void system_input_handle_init(void);
int system_event_map_init(void);
void system_app_enter_poweroff(bool tws_trigger);
void system_app_init(void);

#if defined(CONFIG_SYSTEM_APP_PY32_UART)
void system_app_py32_uart_init(void);
void system_app_py32_reapply_eq(void);
void py32_rhythm_set_data(const uint8_t *bands, uint8_t playing);
#endif

int system_bt_event_callback(uint8_t event, uint8_t* extra, uint32_t extra_len);

int send_message_to_foregroup(uint8_t msg_type, int event_id, void *event_data, int event_data_size);

int system_app_log_transfer(int log_type, int (*traverse_cb)(uint8_t *data, uint32_t max_len));

#endif
