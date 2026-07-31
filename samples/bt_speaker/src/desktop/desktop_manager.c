
#define SYS_LOG_DOMAIN "desktop"
#include <logging/sys_log.h>
#include <mem_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <msg_manager.h>
#include <input_manager.h>
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif
#include <thread_timer.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bt_manager.h>
#include "app_defines.h"
#include "app_ui.h"
#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif
#include <app_launch.h>
#include "desktop_manager.h"
#include <app_tws.h>

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

extern desktop_plugin_t __desktop_plugin_entry_table[];
extern desktop_plugin_t __desktop_plugin_entry_end[];


typedef struct{
	int def_plugin_id;
	int cur_plugin_id;
	int last_plugin_id;
	uint8_t plugin_num;
	uint8_t start:1;
	uint8_t locked;
	uint32_t plugin_bits;
	desktop_plugin_t* cur_plugin;
}desktop_manager_ctx_t;

static desktop_manager_ctx_t desktop_manager;


OS_MUTEX_DEFINE(desktop_manager_mutex);


static desktop_plugin_t *get_plugin_by_id(int plugin_id)
{
	desktop_plugin_t *plugin_entry;

	if (plugin_id != DESKTOP_PLUGIN_ID_NONE) {
		for (plugin_entry = __desktop_plugin_entry_table;
		     plugin_entry < __desktop_plugin_entry_end;
		     plugin_entry++) {
			if (plugin_id == plugin_entry->plugin_id) {
				return plugin_entry;
			}
		}
	}
	return NULL;
}

static inline desktop_manager_ctx_t *_desktop_manager_get_ctx(void)
{
	return &desktop_manager;
}


int desktop_manager_init(int default_plugin_id)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	desktop_plugin_t* plugin = NULL;

	SYS_LOG_INF("%d", default_plugin_id);
	memset(ctx , 0, sizeof(desktop_manager_ctx_t));
	ctx->cur_plugin_id = DESKTOP_PLUGIN_ID_NONE;
	ctx->last_plugin_id = DESKTOP_PLUGIN_ID_NONE;
	ctx->plugin_num = 0;
	ctx->plugin_bits = 0;

	plugin = get_plugin_by_id(default_plugin_id);
	if (!plugin){
		SYS_LOG_INF("plugin %d no Exist.", default_plugin_id);
		return -1;
	}
	ctx->def_plugin_id = default_plugin_id;
	ctx->plugin_bits |=  (1<<default_plugin_id);
	ctx->plugin_num++;
	ctx->start = 1;

	return 0;
}

int desktop_manager_add(int plugin_id)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	desktop_plugin_t* plugin = NULL;
	int ret = 0;
	os_mutex_lock(&desktop_manager_mutex, OS_FOREVER);
	if(!ctx->start){
		os_mutex_unlock(&desktop_manager_mutex);
		return -EINVAL;
	}

	plugin = get_plugin_by_id(plugin_id);
	if (!plugin){
		SYS_LOG_INF("plugin %d no Exist.\n", plugin_id);
		ret = -1;
		goto Exit;
	}

	if (ctx->plugin_bits & (1<<plugin_id)){
		SYS_LOG_INF("plugin %d Already Exist.\n", plugin_id);
		goto Exit;
	}

	ctx->plugin_bits |= (1<<plugin_id);
	ctx->plugin_num ++;
	SYS_LOG_INF("plugin %d add success.\n", plugin_id);

Exit:
	os_mutex_unlock(&desktop_manager_mutex);
	return ret;

}

int desktop_manager_del(int plugin_id)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	desktop_plugin_t* plugin = NULL;

	os_mutex_lock(&desktop_manager_mutex, OS_FOREVER);
	if(!ctx->start){
		os_mutex_unlock(&desktop_manager_mutex);
		return -EINVAL;
	}

	if (ctx->def_plugin_id == plugin_id) {
		SYS_LOG_WRN("default plugin %d.", plugin_id);
		os_mutex_unlock(&desktop_manager_mutex);
		return -EINVAL;
	}

	plugin = get_plugin_by_id(plugin_id);
	if (!plugin){
		SYS_LOG_INF("plugin %d no Exist.", plugin_id);
		goto Exit;
	}

	if ((ctx->plugin_bits & (1<<plugin_id)) == 0){
		goto Exit;
	}

	ctx->plugin_bits &= (~(1<<plugin_id));
	ctx->plugin_num --;

	SYS_LOG_INF("plugin %d delete sucess.\n", plugin_id);

Exit:
	os_mutex_unlock(&desktop_manager_mutex);
	return 0;

}

int desktop_manager_switch(int plugin_id, u32_t switch_mode)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	int ret = 0;

	os_mutex_lock(&desktop_manager_mutex, OS_FOREVER);
	if(!ctx->start){
		ret = -1;
		goto Exit;
	}

	if(ctx->locked){
		SYS_LOG_WRN("locked.");
		ret = -1;
		goto Exit;
	}

	if (switch_mode == DESKTOP_SWITCH_LAST){
		plugin_id = ctx->last_plugin_id;
	}else if(switch_mode == DESKTOP_SWITCH_NEXT){
		if(ctx->plugin_num > 1){
			plugin_id = (ctx->cur_plugin_id + 1) % 32;
			while(!((ctx->plugin_bits)&(1<<plugin_id))){
				plugin_id = (plugin_id + 1)% 32;
			}
		}
	}

	if(plugin_id == ctx->cur_plugin_id){
		SYS_LOG_INF("plugin %d again.", ctx->cur_plugin_id);
		goto Exit;
	}

	if ((ctx->plugin_bits & (1<<plugin_id)) == 0){
		SYS_LOG_ERR("plugin %d no Exist.", plugin_id);
		ret = -1;
		goto Exit;
	}

	ctx->last_plugin_id = ctx->cur_plugin_id;

	if (ctx->cur_plugin){
		SYS_LOG_INF("plugin %d EXIT.", ctx->cur_plugin_id);
		ctx->cur_plugin->exit();
	}

	ctx->cur_plugin_id = plugin_id;

	ctx->cur_plugin =  get_plugin_by_id(plugin_id);
	if (!ctx->cur_plugin){
		SYS_LOG_ERR("plugin %d no Exist.", plugin_id);
		ret = -1;
		goto Exit;
	}

	SYS_LOG_INF("plugin %d Enter.", plugin_id);

#ifdef CONFIG_PLAYTTS
	tts_manager_disable_filter();
#endif

	ctx->cur_plugin->enter(ctx->cur_plugin->p1, ctx->cur_plugin->p2, ctx->cur_plugin->p3);

Exit:
	os_mutex_unlock(&desktop_manager_mutex);

	return ret;
}

int desktop_manager_dump_state(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();

	printk("\nDesktop plugins:\n");
	printk("  plugin_bits: 0x%x\n", ctx->plugin_bits);
	printk("  plugin_num: %d\n", ctx->plugin_num);
	printk("  def_plugin_id: %d\n", ctx->def_plugin_id);
	printk("  cur_plugin_id: %d\n", ctx->cur_plugin_id);
	printk("  last_plugin_id: %d\n", ctx->last_plugin_id);
	printk("  start: %d\n", ctx->start);

	printk("\napp status:\n");
	printk("  tws enable: %d\n", app_tws_status_get_enable());
	printk("  tws mode: %d\n", app_tws_status_get_mode());
	printk("  tws role: %d\n", app_tws_status_get_role());
	printk("  tws connected: %d\n", app_tws_status_get_connected());
	printk("  auracast: %d\n", system_app_get_auracast_mode());

	if(!ctx->start){
		return 0;
	}

	if (ctx->cur_plugin_id == 0xff){
		return 0;
	}

	if(ctx->cur_plugin && ctx->cur_plugin->dump_state)
		return ctx->cur_plugin->dump_state();
	else
		return 0;

}

int desktop_manager_get_plugin_id(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	if(!ctx->start){
		return DESKTOP_PLUGIN_ID_NONE;
	}

	return ctx->cur_plugin_id;
}

int desktop_manager_get_last_plugin_id(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	if(!ctx->start){
		return DESKTOP_PLUGIN_ID_NONE;
	}

	return ctx->last_plugin_id;
}

void desktop_manager_exit(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	if(!ctx->start){
		return;
	}

	if (ctx->cur_plugin){
		ctx->cur_plugin->exit();
		ctx->cur_plugin = NULL;
	}
	ctx->cur_plugin_id = DESKTOP_PLUGIN_ID_NONE;
	ctx->start = 0;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif

	SYS_LOG_INF("exit ok\n");
}

int desktop_manager_enter_app(int plug_id)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();

	SYS_LOG_INF("plug_id:%d\n", plug_id);
	if(!ctx->start){
		return -EINVAL;
	}

	if (ctx->locked){
		SYS_LOG_WRN("locked.");
		return -EINVAL;
	}

	return desktop_manager_switch(plug_id, DESKTOP_SWITCH_CURR);
}

int desktop_manager_exit_app(int plug_id)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();
	int app = desktop_manager_get_plugin_id();

	SYS_LOG_INF("plug_id:%d\n", plug_id);
	if(!ctx->start){
		return -EINVAL;
	}

	if (ctx->locked){
		SYS_LOG_WRN("locked.");
		return -EINVAL;
	}

	if (app != plug_id) {
		SYS_LOG_INF("cur app:%d\n", app);
		return 0;
	}

	if (desktop_manager_switch(0, DESKTOP_SWITCH_LAST)) {
		return desktop_manager_switch(ctx->def_plugin_id, DESKTOP_SWITCH_CURR);
	}

	return 0;
}

int desktop_manager_proc_app_msg(struct app_msg *msg)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();

	if(ctx->start){
		if (ctx->cur_plugin){
			return ctx->cur_plugin->proc_msg(msg);
		}
	}

	return 0;
}

int desktop_manager_lock(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();

	os_mutex_lock(&desktop_manager_mutex, OS_FOREVER);
	if(!ctx->start){
		os_mutex_unlock(&desktop_manager_mutex);
		return -EINVAL;
	}

	SYS_LOG_INF("%d", ctx->locked);
	ctx->locked++;

	os_mutex_unlock(&desktop_manager_mutex);
	return 0;
}

int desktop_manager_unlock(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();

	os_mutex_lock(&desktop_manager_mutex, OS_FOREVER);
	if(!ctx->start){
		os_mutex_unlock(&desktop_manager_mutex);
		return -EINVAL;
	}

	SYS_LOG_INF("%d", ctx->locked);
	if(ctx->locked > 0) {
		ctx->locked--;
	}

	os_mutex_unlock(&desktop_manager_mutex);
	return 0;

}

bool desktop_manager_is_locked(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();

	return (0 == ctx->locked) ? false : true;
}

int desktop_manager_get_default_plugin_id(void)
{
	desktop_manager_ctx_t *ctx = _desktop_manager_get_ctx();

	return ctx->def_plugin_id;
}
