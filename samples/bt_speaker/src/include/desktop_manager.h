
#ifndef _DESKTOP_MANAGER_H_
#define _DESKTOP_MANAGER_H_

typedef enum
{
	DESKTOP_PLUGIN_ID_BR_MUSIC = 0,
	DESKTOP_PLUGIN_ID_BR_CALL = 1,
	DESKTOP_PLUGIN_ID_BMR = 2,
	DESKTOP_PLUGIN_ID_LE_MUSIC = 3,
	DESKTOP_PLUGIN_ID_LE_CALL = 4,
	DESKTOP_PLUGIN_ID_LE_TWS_MUSIC = 5,

	DESKTOP_PLUGIN_ID_LINE_IN = 10,
	DESKTOP_PLUGIN_ID_I2SRX_IN = 11,
	DESKTOP_PLUGIN_ID_SPDIFRX_IN = 12,
	DESKTOP_PLUGIN_ID_MIC_IN = 13,

	DESKTOP_PLUGIN_ID_SDCARD_PLAYER = 20,
	DESKTOP_PLUGIN_ID_USB_PLAYER = 21,
	DESKTOP_PLUGIN_ID_NOR_PLAYER = 22,

	DESKTOP_PLUGIN_ID_DISK_OTA_PRE = 23,
	DESKTOP_PLUGIN_ID_OTA = 24,

	DESKTOP_PLUGIN_ID_UAC = 30,
	DESKTOP_PLUGIN_ID_DEMO = 31,

	DESKTOP_PLUGIN_ID_ATT = 0xf0,

	DESKTOP_PLUGIN_ID_NONE = 0xff,
} desktop_plugin_id_e;

typedef struct desktop_plugin{
	int plugin_id;
	int (*enter)(void *p1, void *p2, void *p3);
	int (*exit)(void);
	int (*proc_msg)(struct app_msg *msg);
	int (*dump_state)(void);
	/** parama1 of app */
	void * p1;
	/** parama2 of app */
	void * p2;
	/** parama3 of app */
	void * p3;
}desktop_plugin_t;

int desktop_manager_init(int default_plugin_id);

typedef enum
{
	DESKTOP_SWITCH_CURR,
	DESKTOP_SWITCH_NEXT,
	DESKTOP_SWITCH_LAST,
} DESKTO_SWITCH_MODE;

int desktop_manager_switch(int plugin_id, u32_t switch_mode);
int desktop_manager_add(int plugin_id);
int desktop_manager_del(int plugin_id);
int desktop_manager_get_state(void);
int desktop_manager_get_plugin_id(void);
int desktop_manager_get_last_plugin_id(void);
int desktop_manager_dump_state(void);
int desktop_manager_proc_app_msg(struct app_msg *msg);
void desktop_manager_exit(void);
int desktop_manager_enter_app(int plug_id);
int desktop_manager_exit_app(int plug_id);
int desktop_manager_lock(void);
int desktop_manager_unlock(void);
bool desktop_manager_is_locked(void);
int desktop_manager_get_default_plugin_id(void);

#define DESKTOP_PLUGIN_DEFINE(id, plugin_enter, plugin_exit, plugin_proc_msg, plugin_status, param1,param2,param3)  \
	const desktop_plugin_t __desktop_plugin_name_##id  \
	__attribute__((__section__(".desktop_plugin_entry")))= {	\
	.plugin_id = id,                          	\
	.enter = plugin_enter,                           \
	.exit  = plugin_exit,                            \
	.proc_msg = plugin_proc_msg,                     \
	.dump_state = plugin_status,                     \
	.p1 = param1,                                    \
	.p2 = param2,                                    \
	.p3 = param3,                                    \
	}                                                \


#endif /*_DESKTOP_MANAGER_H_*/


