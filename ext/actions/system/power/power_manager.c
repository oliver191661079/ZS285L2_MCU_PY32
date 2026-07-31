/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file power manager interface
 */
#if defined(CONFIG_SYS_LOG)
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "power"
#endif
#include <logging/sys_log.h>
#include <os_common_api.h>
#include <soc.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <sys_event.h>
#include <sys_monitor.h>
#include <property_manager.h>
#include <power_supply.h>
#include <string.h>
#ifdef CONFIG_POWER_SMART_CONTROL
#include <volume_manager.h>
#endif

#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#ifdef CONFIG_CONSOLE_SHELL
#include <stdlib.h>
#include <shell/shell.h>
static int bat_mode = 0;
#endif

#define DEFAULT_BOOTPOWER_LEVEL	3100000
#define DEFAULT_NOPOWER_LEVEL	    3400000
#define DEFAULT_LOWPOWER_LEVEL	3600000
#define DEFAULT_OTA_POWER_LEVEL	3700000

//This period should be larger than CONFIG_AUTO_STANDBY_TIME_SEC to allow standby.
#define DEFAULT_REPORT_PERIODS	(90*1000)

struct power_manager_info {
	struct device *dev;
	bat_charge_callback_t cb;
	bat_charge_event_t event;
	bat_charge_event_para_t *para;
	int nopower_level;
	int lowpower_level;
	int ota_power_level;
	int current_vol;
	int current_cap;
	int slave_vol;
	int slave_cap;
	int battary_changed;
#ifdef CONFIG_POWER_SMART_CONTROL
	int smart_control;
#endif
	uint32_t charge_full_flag;
	uint32_t report_timestamp;
};

struct power_manager_info *power_manager = NULL;

void power_supply_report(bat_charge_event_t event, bat_charge_event_para_t *para)
{
	if (!power_manager) {
		return;
	}
	SYS_LOG_INF("event %d\n", event);
	switch (event) {
	case BAT_CHG_EVENT_DC5V_IN:
		break;
	case BAT_CHG_EVENT_DC5V_OUT:
		break;
	case BAT_CHG_EVENT_CHARGE_START:
	case BAT_CHG_EVENT_CHARGE_STOP:
		power_manager->battary_changed = 1;
		break;
	case BAT_CHG_EVENT_CHARGE_FULL:
		power_manager->charge_full_flag = 1;
		break;

	case BAT_CHG_EVENT_VOLTAGE_CHANGE:
		SYS_LOG_INF("voltage change %u uV\n", para->voltage_val);
		power_manager->current_vol = para->voltage_val;
		break;

	case BAT_CHG_EVENT_CAP_CHANGE:
		SYS_LOG_INF("cap change %u\n", para->cap);
		power_manager->current_cap = para->cap;
		power_manager->battary_changed = 1;
		break;

	default:
		break;
	}
}



static int get_system_bat_info(int property)
{
	union power_supply_propval val;

	int ret;

	if (!power_manager || !power_manager->dev) {
		SYS_LOG_ERR("dev not found\n");
		return -ENODEV;
	}

	ret = power_supply_get_property(power_manager->dev, property, &val);
	if (ret < 0) {
		SYS_LOG_ERR("get property err %d\n", ret);
		return -ENODEV;
	}

	return val.intval;
}

int power_manager_get_battery_capacity(void)
{
#ifdef CONFIG_TWS
	int report_cap = get_system_bat_info(POWER_SUPPLY_PROP_CAPACITY);
	if (bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER) {
		if (report_cap > power_manager->slave_cap)
			report_cap = power_manager->slave_cap;
	}
	return report_cap;
#else
	return get_system_bat_info(POWER_SUPPLY_PROP_CAPACITY);
#endif
}

int power_manager_get_charge_status(void)
{
	return get_system_bat_info(POWER_SUPPLY_PROP_STATUS);
}

bool power_manager_get_battery_is_full(void)
{
	return (power_manager_get_charge_status() == POWER_SUPPLY_STATUS_FULL) ? true : false;
}

int power_manager_get_battery_vol(void)
{
	return get_system_bat_info(POWER_SUPPLY_PROP_VOLTAGE_NOW);
}

int power_manager_get_dc5v_status(void)
{
	return get_system_bat_info(POWER_SUPPLY_PROP_DC5V);
}

int power_manager_set_slave_battery_state(int capacity, int vol)
{
	power_manager->slave_vol = vol;
	power_manager->slave_cap = capacity;
	power_manager->battary_changed = 1;
	SYS_LOG_INF("vol %dmv cap %d\n", vol, capacity);
	return 0;
}

bool power_manager_check_is_no_power(void)
{
	if (power_manager_get_dc5v_status())
		return false;

	if (power_manager_get_battery_vol() <= power_manager->nopower_level) {
		SYS_LOG_INF("%d %d too low", power_manager_get_battery_vol(), power_manager_get_battery_capacity());
		return true;
	}

	return false;
}

bool power_manager_check_ota_power(void)
{
	if (power_manager_get_dc5v_status())
		return true;

	if (power_manager_get_battery_vol() > power_manager->ota_power_level) {
		return true;
	}

	return false;
}


int power_manager_sync_slave_battery_state(void)
{
	uint32_t send_value;

#ifdef CONFIG_BT_TWS_US281B
	send_value = power_manager->current_cap / 10;
#else
	send_value = (power_manager->current_cap << 24) | power_manager->current_vol;
#endif

#ifdef CONFIG_TWS
	bt_manager_tws_send_event(TWS_BATTERY_EVENT, send_value);
#endif

	return 0;
}

#ifdef CONFIG_POWER_SMART_CONTROL
/*
The unit shall support smart post Gain control for reducing power 
consumption. 

If remain battery capacity is lower than 15%, the post Gain is 
reduced by 0.5 db every 30s. And the total reduction is 2 db.

When above is triggered and the remain battery capacity is higher 
than 15%, the post Gain is increased by 0.5db every 30s till the 
reduction is increased.
*/

#define BAT_CAPACITY_THRESHOLD 15
#define GAIN_STEP 5
#define GAIN_TARGET -20
static void power_manager_smart_control(void)
{
	static u32_t count = 0;
	static int increase = 0;
	bool last = false;
	bool low_bat = false;

#ifdef CONFIG_CONSOLE_SHELL
	if (bat_mode == 0) {
		if (power_manager->current_cap < BAT_CAPACITY_THRESHOLD) {
			low_bat = true;
		} else {
			low_bat  = false;
		}
	} else {
		if(bat_mode == 1) {
			low_bat = true;
		} else {
			low_bat = false;
		}
	}
#else
	if (power_manager->current_cap < BAT_CAPACITY_THRESHOLD) {
		low_bat = true;
	} else {
		low_bat  = false;
	}
#endif


	if (power_manager->battary_changed) {
		SYS_LOG_INF("Gain %d, inc %d, low_bat %d\n", power_manager->smart_control, increase, low_bat);
		if (low_bat && power_manager->smart_control > GAIN_TARGET) {
			increase = -1;
			count = 0xFFFFFFFF;
		}

		if (!low_bat && power_manager->smart_control < 0) {
			increase = 1;
			count = 0xFFFFFFFF;
		}
	}

	if (increase != 0) {
		if(count >= (30*1000) / CONFIG_MONITOR_PERIOD) {
			if (increase == 1) {
				if(power_manager->smart_control < 0) {
					power_manager->smart_control += GAIN_STEP;
					if(power_manager->smart_control >= 0) {
						power_manager->smart_control = 0;
						last = true;
					}
				}
			}

			if (increase == -1) {
				if(power_manager->smart_control > GAIN_TARGET) {
					power_manager->smart_control -= GAIN_STEP;
					if(power_manager->smart_control <= GAIN_TARGET) {
						power_manager->smart_control = GAIN_TARGET;
						last = true;
					}
				}
			}

			SYS_LOG_INF("Gain %d, inc %d, last %d\n", power_manager->smart_control, increase, last);
			system_player_smart_control_volume_set(power_manager->smart_control);
			count = 0;

			if(last) {
				increase = 0;
			}
		}
		count++;
	}
}
#endif

static int _power_manager_work_handle(void)
{
	if (!power_manager)
		return -ESRCH;

#ifdef CONFIG_POWER_SMART_CONTROL
	power_manager_smart_control();
#endif

	if (power_manager->battary_changed) {
		power_manager->battary_changed = 0;
#ifdef CONFIG_BT_HFP_HF
	#ifdef CONFIG_TWS
		if (bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE) {
			power_manager_sync_slave_battery_state();
		} else if (bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER) {
			int report_cap = power_manager->current_cap;

			if (report_cap > power_manager->slave_cap)
				report_cap = power_manager->slave_cap;

			bt_manager_hfp_battery_report(BT_BATTERY_REPORT_VAL, report_cap);
		} else {
			bt_manager_hfp_battery_report(BT_BATTERY_REPORT_VAL, power_manager->current_cap);
		}
	#else
		bt_manager_hfp_battery_report(BT_BATTERY_REPORT_VAL, power_manager->current_cap);
	#endif
#endif
		sys_event_send_message(MSG_BAT_CHARGE_EVENT);
	}

	if (power_manager_get_dc5v_status())
		return 0;

	if (power_manager->charge_full_flag) {
		power_manager->charge_full_flag = 0;
		sys_event_notify(SYS_EVENT_CHARGE_FULL);
	}

	if (power_manager->current_vol <= power_manager->nopower_level) {
		if((os_uptime_get_32() - power_manager->report_timestamp) >=  DEFAULT_REPORT_PERIODS
			|| !power_manager->report_timestamp) {
			SYS_LOG_INF("%d %d too low\n", power_manager->current_vol, power_manager->slave_vol);
			sys_event_notify(SYS_EVENT_BATTERY_TOO_LOW);
			power_manager->report_timestamp = os_uptime_get_32();
		}
		return 0;
	}

	if ((power_manager->current_vol <= power_manager->lowpower_level)
		&& ((os_uptime_get_32() - power_manager->report_timestamp) >=  DEFAULT_REPORT_PERIODS
			|| !power_manager->report_timestamp)) {
		SYS_LOG_INF("%d %d low\n", power_manager->current_vol, power_manager->slave_vol);
	#ifdef CONFIG_TWS
		if (bt_manager_tws_get_dev_role() != BTSRV_TWS_SLAVE) {
			sys_event_notify(SYS_EVENT_BATTERY_LOW);
		}
	#else
		sys_event_notify(SYS_EVENT_BATTERY_LOW);
	#endif
		power_manager->report_timestamp = os_uptime_get_32();
	}

	return 0;
}

static struct power_manager_info global_power_manager;

int power_manager_init(void)
{
	power_manager = &global_power_manager;

	memset(power_manager, 0, sizeof(struct power_manager_info));

	power_manager->dev = (struct device *)device_get_binding("battery");
	if (!power_manager->dev) {
		SYS_LOG_ERR("dev not found\n");
		return -ENODEV;
	}

	if ((power_manager_get_battery_vol() <= DEFAULT_BOOTPOWER_LEVEL)
			&& (!power_manager_get_dc5v_status())) {
		SYS_LOG_INF("no power ,shundown: %d\n", power_manager_get_battery_vol());
		sys_pm_poweroff();
		return 0;
	}


	power_supply_register_notify(power_manager->dev, power_supply_report);
#ifdef CONFIG_PROPERTY
	power_manager->lowpower_level = property_get_int(CFG_LOW_POWER_WARNING_LEVEL, DEFAULT_LOWPOWER_LEVEL);
	power_manager->nopower_level = property_get_int(CFG_SHUTDOWN_POWER_LEVEL, DEFAULT_NOPOWER_LEVEL);
	power_manager->ota_power_level = property_get_int(CFG_OTA_POWER_LEVEL, DEFAULT_OTA_POWER_LEVEL);
#else
	power_manager->lowpower_level = DEFAULT_LOWPOWER_LEVEL;
	power_manager->nopower_level = DEFAULT_NOPOWER_LEVEL;
	power_manager->ota_power_level = DEFAULT_OTA_POWER_LEVEL;
#endif
	power_manager->current_vol = power_manager_get_battery_vol();
	power_manager->current_cap = power_manager_get_battery_capacity();
	power_manager->slave_vol = 4200000;
	power_manager->slave_cap = 100;
	power_manager->report_timestamp = 0;

	sys_monitor_add_work(_power_manager_work_handle);

	return 0;
}

#ifdef CONFIG_CONSOLE_SHELL
static int shell_set_bat_mode(int argc, char *argv[])
{
	if (argc == 2) {
		bat_mode = strtoul(argv[1], (char **)NULL, 10);
		power_manager->battary_changed = 1;
	}

	printk("bat mode: %d\n", bat_mode);

	return 0;
}

static const struct shell_cmd power_commands[] = {
	{"bat", shell_set_bat_mode, "set battery mode [0,1,2]. 0-auto, 1-manual low, 2-manual high"},
	{NULL, NULL, NULL}
};

SHELL_REGISTER("power", power_commands);
#endif
