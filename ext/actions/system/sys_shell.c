/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system shell
 */

#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "system_shell"
#include <logging/sys_log.h>

#include <os_common_api.h>
#include <mem_manager.h>
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <shell/shell.h>
#include <stdlib.h>
#include <limits.h>
#include <stack_backtrace.h>
#include <property_manager.h>

#define SYSTEM_SHELL "system"

static int shell_set_config(int argc, char *argv[])
{
	int ret = 0;

	if (argc < 2) {
		printk("argc <\n");
		return 0;
	}

#ifdef CONFIG_PROPERTY
	if (argc < 3) {
		ret = property_set(argv[1], NULL, 0);
	} else {
		ret = property_set(argv[1], argv[2], strlen(argv[2]));
	}
#endif
	if (ret < 0) {
		SYS_LOG_INF("%s failed %d\n", argv[1], ret);
		return -1;
	}
#ifdef CONFIG_PROPERTY
	property_flush(NULL);
#endif
	SYS_LOG_INF("%s : %s ok\n", argv[1], argv[2]);
	return 0;
}
extern const struct  slabs_info app_slab;
extern const struct  slabs_info system_slab;
static int shell_dump_meminfo(int argc, char *argv[])
{
	int index = -1;

	if (argv[1] != NULL) {
		index = atoi(argv[1]);
	}
#ifdef CONFIG_USED_MEM_SLAB
	mem_dump((struct slabs_info *)&system_slab, index);
#endif

#ifdef CONFIG_APP_USED_MEM_SLAB
	mem_dump((struct slabs_info *)&app_slab, index);
#endif

#ifdef CONFIG_APP_USED_MEM_PAGE
	mem_manager_dump();
#endif
	return 0;
}

extern void soc_set_hosc_cap(int cap);
extern int soc_get_hosc_cap(void);

static int shell_set_hosc_cap(int argc, char *argv[])
{
	int cap;

	if (argc < 2) {
		printk("argc < 2\n");
		return 0;
	}

	cap = atoi(argv[1]);

	if (cap > 1 && cap <= 240) {
		soc_set_hosc_cap(cap);
	}

	cap = soc_get_hosc_cap();
	SYS_LOG_INF("read host cap : %d.%d pf\n", cap/10, cap%10);
	return 0;

}

static const struct shell_cmd system_commands[] = {
	{ "dumpmem", shell_dump_meminfo, "dump mem info" },
	{ "set_config", shell_set_config, "set system config " },
	{ "set_hosc_cap", shell_set_hosc_cap, "set hosc cap " },
	{ NULL, NULL, NULL }
};

SHELL_REGISTER(SYSTEM_SHELL, system_commands);
