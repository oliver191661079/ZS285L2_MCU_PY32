
#ifndef _APP_COMMON_H_
#define _APP_COMMON_H_

#define MAX_AUDIO_VOL_LEVEL 16

void system_app_init_bte_ready(void);

void btmusic_dump_app_data(void);
void auxtws_dump_app_data(void);
void btcall_dump_app_data(void);
void lemusic_dump_app_data(void);
void le_audio_dump_app_data(void);
void soundbar_dump_app_data(void);
void bms_br_dump_app_data(void);
void bmr_dump_app_data(void);
void bms_uac_dump_app_data(void);
void bms_audioin_dump_app_data(void);
void usound_dump_app_data(void);
void demo_dump_app_data(void);
void audio_input_dump_app_data(void);

int send_message_to_system(uint8_t msg_type, uint8_t cmd, uint8_t reserved);

#endif
