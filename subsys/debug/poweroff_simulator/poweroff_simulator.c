/*
 * Copyright (c) 2025 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file powerdown_simulator.c
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/types.h>
#include <linker/linker-defs.h>
#include <errno.h>
#include <init.h>
#include <device.h>
#include <debug/data_export.h>
#include <mem_manager.h>
#include <crc.h>
#ifdef CONFIG_WATCHDOG
#include <watchdog_hal.h>
#endif

#include <soc.h>
#include <shell/shell.h>
#include <debug/poweroff_simulator.h>

typedef struct {
    sys_slist_t points;
    struct k_delayed_work reboot_work;
    bool enabled;
    bool pending;
} poweroff_sim_t;


static poweroff_sim_t g_power_down_sim;

extern const test_point_info_t __start_test_points[];
extern const test_point_info_t __stop_test_points[];

static void poweroff_sim_reboot(void)
{
	sys_pm_reboot(0);
}

static void reboot_work_handler(struct k_work *work)
{
    poweroff_sim_reboot();
}


const test_point_info_t* find_test_point(uint16_t uid)
{
    const test_point_info_t* start = __start_test_points;
    const test_point_info_t* end = __stop_test_points;

    for(; start < end; start++){
        if (start->uid == uid) {
            return start;
        }
    }

    return NULL;
}

int add_test_point(uint16_t uid)
{
    test_point_t *exist;
    SYS_SLIST_FOR_EACH_CONTAINER(&g_power_down_sim.points, exist, node) {
        if (exist->info->uid == uid) {
            printk("Added TP:0x%04X\n", uid);
            return -EALREADY;
        }
    }

    const test_point_info_t *info = find_test_point(uid);
    if (!info) {
        printk("Test point 0x%04X not found\n", uid);
        return -ENOENT;
    }

    test_point_t *tp = mem_malloc(sizeof(test_point_t));
    if (!tp) {
        return -ENOMEM;
    }

    tp->info = info;
    tp->ctx.mode = 0;
    tp->ctx.delay_ms = 0;
    tp->ctx.inject_value = 0;

    sys_slist_append(&g_power_down_sim.points, (sys_snode_t*)tp);
    printk("Added TP:0x%04X [%s]\n", uid, info->desc);
    return 0;
}

int remove_test_point(uint16_t uid)
{
    test_point_t *prev = NULL;
    test_point_t *current;

    SYS_SLIST_FOR_EACH_CONTAINER(&g_power_down_sim.points, current, node) {
        if (current->info->uid == uid) {
            sys_slist_remove(&g_power_down_sim.points,
                           (sys_snode_t*)prev,
                           (sys_snode_t*)current);
            mem_free(current);
            printk("Removed TP:0x%04X\n", uid);
            return 0;
        }
        prev = current;
    }

    printk("Test point 0x%04X not found\n", uid);
    return -ENOENT;
}



void _trigger_test_point(uint16_t uid, void *param)
{
	test_point_t *tp;

    if (!g_power_down_sim.enabled || g_power_down_sim.pending){
		return;
	}

    SYS_SLIST_FOR_EACH_CONTAINER(&g_power_down_sim.points, tp, node) {
        if (tp->info->uid == uid) {

			printk("[TEST_EVENT] UID=0x%04X\n", uid);

			//执行自定义函数,比如打印额外的日志
            if (tp->info->handler) {
                if(tp->info->handler(tp, param) == 0){
					return;
				}
            }

            switch (tp->ctx.mode) {
            case 1:
                printk("[TEST] Immediate reboot @ %s\n", tp->info->desc);
                poweroff_sim_reboot();
                break;
            case 2:
                printk("[TEST] Delayed reboot %dms @ %s\n",
                      tp->ctx.delay_ms, tp->info->desc);
				g_power_down_sim.pending = true;
                k_delayed_work_submit(&g_power_down_sim.reboot_work, K_MSEC(tp->ctx.delay_ms));
                break;
            }
            return;
        }
    }
    //printk("[TEST][ERR]Test point 0x%04X not found\n", uid);
}


int poweroff_sim_init(void)
{
	memset(&g_power_down_sim, 0, sizeof(g_power_down_sim));

	k_delayed_work_init(&g_power_down_sim.reboot_work, reboot_work_handler);

	printk("poweroff sim init\n");

	k_sleep(2000);

	return 0;
}


#define POWEROFF_TEST "pwtest"


static int shell_poweroff_sim_enable(int argc, char *argv[])
{
	g_power_down_sim.enabled = true;

	g_power_down_sim.pending = false;

	printk("Power off module enabled\n");
	return 0;
}

static int shell_poweroff_sim_disable(int argc, char *argv[])
{
	g_power_down_sim.enabled = false;

	printk("Power off module disabled\n");
	return 0;
}

static int shell_poweroff_sim_list(int argc, char *argv[])
{
	int active_tp_cnt = 0;
	int inactive_tp_cnt = 0;

    printk("\n%-8s %-10s %-10s %-10s %s\n",
          "UID", "STATUS", "MODE", "DELAY(ms)", "DESCRIPTION");
    printk("------------------------------------------------------------------------\n");

    const test_point_info_t *const_ptr;
    for (const_ptr = __start_test_points; const_ptr < __stop_test_points; const_ptr++) {
        bool is_active = false;
        test_point_t *active_tp;

        SYS_SLIST_FOR_EACH_CONTAINER(&g_power_down_sim.points, active_tp, node) {
            if (active_tp->info->uid == const_ptr->uid) {
                is_active = true;
                break;
            }
        }

        printk("0x%04X  [%-5s] %-9s %-10s \"%s\"\n",
              const_ptr->uid,
              is_active ? "YES" : "NO",
              "--", "--",  // 未激活时模式/延迟显示为--
              const_ptr->desc);

		if(!is_active)
			inactive_tp_cnt++;
    }

    if (!sys_slist_is_empty(&g_power_down_sim.points)) {
        printk("\nActive Configurations:\n");
        printk("%-8s %-6s %-10s %-40s\n", "UID", "MODE", "DELAY(ms)", "PARAM");

        test_point_t *active_tp;
        SYS_SLIST_FOR_EACH_CONTAINER(&g_power_down_sim.points, active_tp, node) {
            printk("0x%04X  %-6d %-10d %-40d\n",
                  active_tp->info->uid,
                  active_tp->ctx.mode,
                  active_tp->ctx.delay_ms,
                  active_tp->ctx.inject_value);
			active_tp_cnt++;
        }
    }

    printk("\nTotal: %d registered | %d activated\n", inactive_tp_cnt + active_tp_cnt, active_tp_cnt);

    return 0;
}


static int shell_poweroff_sim_add(int argc, char *argv[])
{
    if (argc < 2) {
        printk("Usage: add <uid_hex>\n");
        return -EINVAL;
    }

    uint16_t uid = (uint16_t)strtoul(argv[1], NULL, 16);
    return add_test_point(uid);
}

static int shell_poweroff_sim_remove(int argc, char *argv[])
{
    if (argc < 2) {
        printk("Usage: remove <uid_hex>\n");
        return -EINVAL;
    }

    uint16_t uid = (uint16_t)strtoul(argv[1], NULL, 16);
    return remove_test_point(uid);
}


static int shell_poweroff_sim_config(int argc, char *argv[])
{
	test_point_t *tp;

    if (argc < 4) {
        printk("Usage: config <uid_hex> <mode> [delay_ms]\n");
        return -EINVAL;
    }

    const uint16_t uid = (uint16_t)strtoul(argv[1], NULL, 16);
    const uint8_t mode = (uint8_t)strtoul(argv[2], NULL, 10);

    if (mode > 2) {
        printk("Invalid mode: 0=Disable, 1=Immediate, 2=Delayed\n");
        return -EINVAL;
    }

    uint32_t delay_ms = 0;

    if (mode == 2) {
        if (argc < 4) {
            printk("Delay required for mode 2\n");
            return -EINVAL;
        }
        delay_ms = (uint32_t)strtoul(argv[3], NULL, 10);
    }

    SYS_SLIST_FOR_EACH_CONTAINER(&g_power_down_sim.points, tp, node) {
        if (tp->info->uid == uid) {
            tp->ctx.mode = mode;
            tp->ctx.delay_ms = delay_ms;
            printk("Configured 0x%04X: mode=%d, delay=%dms\n", uid, mode, delay_ms);
            return 0;
        }
    }

    printk("Test point 0x%04X not found!\n", uid);
    return -ENOENT;

}

static int shell_poweroff_sim_inject(int argc, char *argv[])
{
	test_point_t *tp;

    uint32_t uid = strtoul(argv[1], NULL, 16);
    uint32_t value = strtoul(argv[2], NULL, 10);

    SYS_SLIST_FOR_EACH_CONTAINER(&g_power_down_sim.points, tp, node) {
        if (tp->info->uid == uid) {
            tp->ctx.inject_value = value;
            printk("UID 0x%X inject_value=%d\n", uid, value);
            return 0;
        }
    }
    return -ENOENT;
}

static const struct shell_cmd poweroff_sim_commands[] =
{
    {"enable",  shell_poweroff_sim_enable,  "Enable poweroff simulator"},

    {"disable", shell_poweroff_sim_disable, "Disable poweroff simulator"},

    {"add",     shell_poweroff_sim_add,     "Add test point: add <uid_hex>"},

    {"remove",  shell_poweroff_sim_remove,  "Remove test point: remove <uid_hex>"},

    {"list",    shell_poweroff_sim_list,    "List active test points"},

    {"config",  shell_poweroff_sim_config,  "Configure test point: config <uid> <mode> [delay]"},

    {"inject",  shell_poweroff_sim_inject,  "Inject value: inject <uid> <value>"},

    {NULL, NULL, NULL}

};

SHELL_REGISTER(POWEROFF_TEST, poweroff_sim_commands);



