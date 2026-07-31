#ifndef _SELFAPP_API_H
#define _SELFAPP_API_H

#include <stdbool.h>
#include <zephyr/types.h>

typedef int (*p_logsrv_callback_t)(int log_type, int (*traverse_cb)(uint8_t *data, uint32_t max_len));

extern int selfapp_init(p_logsrv_callback_t cb);
extern int selfapp_deinit(void);


/*
 Send message to selfapp thread (thread timer of system app thread)
*/
void selfapp_send_msg(u8_t type, u8_t cmd, u8_t param, int val);

/*
Mobile app asks for adv data of manuafacturer type.
*/
extern u8_t selfapp_get_manufacturer_data(u8_t *buf);

/*
When enter/exit auracast(bmr/bms), it should notify mobile app.
*/
extern void selfapp_notify_role(void);

/*
Mobile app sets the playback channel mode in Stereo group setting.
return: output channel mode
	0 - Stereo
	1 - channel left
	2 - channel right
*/
u8_t selfapp_get_channel(void);

/*
Selfapp connect event action.
*/
void selfapp_on_connect_event(u8_t connect, u8_t connect_type, u32_t handle);

/*
Return: [true, false].
	ture - playtime boost on
	false- playtime boost off
*/
bool selfapp_eq_is_playtimeboost(void);

/*
Return: [true, false].
	ture - Use default EQ. (Preset EQ)
	false- Use not default EQ. (Customer EQ)
*/
bool selfapp_eq_is_default(void);

/*
Return: EQ Category index, 4bits (0x0-0xF)
*/
u8_t selfapp_eq_get_index(void);

/*
Return:
	<0 - fail
	0  - Success
*/
int selfapp_report_bat(void);

/*
Reset nvram storage data of selfapp.
*/
void selfapp_config_reset(void);

/*
Switch on/off auracast eq mode.
mode:
	0 - auracast eq off
	others - auracast eq on
*/
void selfapp_eq_cmd_switch_auracast(int mode);

/*
Switch on/off dsp hardware test.
enable:
	false - normal mode
	true - hardware test mode.
*/
void selfapp_eq_cmd_switch_hw_test(bool enable);

/* Load selfapp configs. */
void selfapp_config_init(void);
#endif
