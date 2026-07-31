/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Magic System Request Key Hacks
 *
 */

#include <kernel.h>
#include <uart.h>
#include <misc/printk.h>
#include <stack_backtrace.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SYSRQ

#ifdef CONFIG_UART_CONSOLE_DEFAULT_DISABLE
#include <drivers/console/uart_console.h>
#endif

struct sysrq_key_op {
	char key;
	char reserved[3];

	void (*handler)(int);
	char *help_msg;
	char *action_msg;
};

#ifdef CONFIG_STACK_BACKTRACE
static void sysrq_handle_show_stack(int key)
{
	show_all_threads_stack();
}
#endif

static void sysrq_handle_switch_jtag(int key)
{
#ifdef CONFIG_SOC_SERIES_ROCKY
	/* switch to jtag group 3, GPIO3/4/5/7 */
	sys_write32((sys_read32(0xc009002c) & ~0x60000000) | 0x40000000, 0xc009002c);
#endif
}

#include <soc_pm.h>
static void sysrq_handle_reboot_device(int key)
{
	sys_pm_reboot(0);
}

static struct sysrq_key_op sysrq_key_table[] = {
#ifdef CONFIG_STACK_BACKTRACE
	{
		.key		= 't',
		.handler	= sysrq_handle_show_stack,
		.help_msg	= "show-thread-states(t)",
		.action_msg	= "Show State:",
	},
#endif

	{
		.key		= 'j',
		.handler	= sysrq_handle_switch_jtag,
		.help_msg	= "jtag(j)",
		.action_msg	= "Switch JTAG",
	},

	{
		.key		= 'r',
		.handler	= sysrq_handle_reboot_device,
		.help_msg	= "reboot device(b)",
		.action_msg	= "reboot device",
	},
};

/* magic sysrq key: CTLR + 'b' 'r' 'e' 'a' 'k' */
static const char sysrq_breakbuf[] = {0x02, 0x12, 0x05, 0x01, 0x0b};
static int sysrq_break_idx;

static struct sysrq_key_op* sysrq_get_key_op(char key)
{
	int i, cnt;

	cnt = ARRAY_SIZE(sysrq_key_table);

	for (i = 0; i < cnt; i++) {
		if (key == sysrq_key_table[i].key) {
			return &sysrq_key_table[i];
		}
	}

	return NULL;
}

static void handle_sysrq_main(struct device * port)
{
	struct sysrq_key_op* op_p;
	int i;
	char key;

#ifdef CONFIG_UART_CONSOLE_DEFAULT_DISABLE
	/* enable the uart console */
	uart_console_hook_install();
#endif

	for(;;) {
		ACT_LOG_ID_INF(ALF_STR_handle_sysrq_main__WAIT_SYSRQ_KEYN, 0);
		uart_poll_in(port, &key);
		if (key == 'q') {
			ACT_LOG_ID_INF(ALF_STR_handle_sysrq_main__SYSRQ_EXITN, 0);
			break;
		}

		op_p = sysrq_get_key_op(key);
		if (op_p) {
			printk("%s\n", op_p->action_msg);
			op_p->handler(key);
		} else {
			ACT_LOG_ID_INF(ALF_STR_handle_sysrq_main__HELP__, 0);
			for (i = 0; i < ARRAY_SIZE(sysrq_key_table); i++) {
				printk("%s ", sysrq_key_table[i].help_msg);
			}
			ACT_LOG_ID_INF(ALF_STR_handle_sysrq_main__N, 0);
		}
	}
}

void uart_handle_sysrq_char(struct device * port, char c)
{
	if (c == sysrq_breakbuf[sysrq_break_idx])
	{
		sysrq_break_idx++;
		if (sysrq_break_idx == sizeof(sysrq_breakbuf)) {
			sysrq_break_idx = 0;
			handle_sysrq_main(port);
		}
	} else {
		sysrq_break_idx = 0;
	}
}
