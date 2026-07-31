/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <misc/printk.h>
#include <shell/shell.h>
#include <init.h>
#include <stdlib.h>
#include <string.h>
#include <debug/object_tracing.h>
#if defined(CONFIG_KALLSYMS)
#include <kallsyms.h>
#endif
#if defined(CONFIG_SYS_LOG)
#include <logging/sys_log.h>
#endif

#include <system_recovery.h>
#include <stack_backtrace.h>
#include <mpu_acts.h>

#define SHELL_KERNEL "kernel"

#define SHELL_MAX_MEM_COUNT 0x800000

static int shell_cmd_version(int argc, char *argv[])
{
	u32_t version = sys_kernel_version_get();

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	printk("Zephyr version %d.%d.%d\n",
	       SYS_KERNEL_VER_MAJOR(version),
	       SYS_KERNEL_VER_MINOR(version),
	       SYS_KERNEL_VER_PATCHLEVEL(version));
	return 0;
}

static int shell_cmd_uptime(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	printk("uptime: %u ms\n", k_uptime_get_32());

	return 0;
}

static int shell_cmd_cycles(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	printk("cycles: %u hw cycles\n", k_cycle_get_32());

	return 0;
}

#if defined(CONFIG_THREAD_MONITOR)
int shell_cmd_tasks(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	struct k_thread *thread_list = NULL;

	printk("tasks:\n");

	thread_list   = (struct k_thread *)SYS_THREAD_MONITOR_HEAD;
	while (thread_list != NULL) {
		printk("%s%p:   options: 0x%x state %d priority: %d \n",
		       (thread_list == k_current_get()) ? "*" : " ",
		       thread_list,
		       thread_list->base.user_options, thread_list->base.thread_state,
		       k_thread_priority_get(thread_list));
		thread_list = (struct k_thread *)SYS_THREAD_MONITOR_NEXT(thread_list);
	}
	return 0;
}
#endif


#if defined(CONFIG_INIT_STACKS)
static int shell_cmd_stack(int argc, char *argv[])
{
	k_call_stacks_analyze();
	return 0;
}

//#if defined(CONFIG_MIPS) && defined(CONFIG_THREAD_STACK_INFO)
#if defined(CONFIG_THREAD_STACK_INFO)
#include <stdio.h>	/* for snprintf() */
#include <misc/stack.h>	/* for stack_analyze() */

extern const char *task_evt2str(int num);

uint32_t mem_is_ram_data(u32_t addr);

static int shell_cmd_stacks_usage(int argc, char *argv[])
{
#ifdef CONFIG_THREAD_MONITOR
	struct k_thread *thread_list = NULL;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	printk("Stacks usage:\n");

	thread_list   = (struct k_thread *)SYS_THREAD_MONITOR_HEAD;
	while (thread_list != NULL) {

		if(!mem_is_ram_data((uint32_t)thread_list)){
			break;
		}
#ifdef CONFIG_ENABLE_TRACE_EVTTOSTR
		printk("%p %s %d", thread_list, task_evt2str(k_thread_priority_get(thread_list)), k_thread_priority_get(thread_list));
#else
		printk("%p %d", thread_list, k_thread_priority_get(thread_list));
#endif
		if(!mem_is_ram_data(thread_list->stack_info.start)){
			break;
		}

		if(!mem_is_ram_data(thread_list->stack_info.start + thread_list->stack_info.size)){
			break;
		}

		stack_analyze(NULL, (const char *)thread_list->stack_info.start,
			 thread_list->stack_info.size);

		thread_list = (struct k_thread *)SYS_THREAD_MONITOR_NEXT(thread_list);
	}

	STACK_ANALYZE("interrupt", _interrupt_stack);
#endif
	return 0;
}

void kernel_dump_all_thread_stack_usage(void)
{
	shell_cmd_stacks_usage(0, NULL);
}

#endif

#endif

#if defined(CONFIG_CMD_MEMORY)
#define DISP_LINE_LEN	16
static int do_mem_mw(int width, int argc, char * const argv[])
{
	unsigned long writeval;
	unsigned long addr, count;
	void *buf;

	if (argc < 3)
		return -EINVAL;

	addr = strtoul(argv[1], NULL, 16);
	writeval = strtoul(argv[2], NULL, 16);

	if (argc == 4)
		count = strtoul(argv[3], NULL, 16);
	else
		count = 1;

    if (count <= 0 || count >= SHELL_MAX_MEM_COUNT) {
        printk("Invalid count: %d\n", (u32_t)count);
        return -EINVAL;
    }

	buf = (void *)addr;
	while (count-- > 0) {
		if (width == 4)
			*((uint32_t *)buf) = (uint32_t)writeval;
		else if (width == 2)
			*((uint16_t *)buf) = (uint16_t)writeval;
		else
			*((uint8_t *)buf) = (uint8_t)writeval;
		buf += width;
	}

	return 0;
}

static int do_mem_md(int width, int argc, char *argv[])
{
	unsigned long addr;
	int count;

	if (argc < 2)
		return -EINVAL;

	addr = strtoul(argv[1], NULL, 16);

	if (argc == 3)
		count = strtoul(argv[2], NULL, 16);
	else
		count = 1;

    if (count <= 0 || count >= SHELL_MAX_MEM_COUNT) {
        printk("Invalid count: %d\n", (u32_t)count);
        return -EINVAL;
    }

	print_buffer((void *)addr, width, count, DISP_LINE_LEN / width, -1);

	return 0;
}

static int shell_cmd_mdw(int argc, char *argv[])
{
	return do_mem_md(4, argc, argv);
}

static int shell_cmd_mdh(int argc, char *argv[])
{
	return do_mem_md(2, argc, argv);
}

static int shell_cmd_mdb(int argc, char *argv[])
{
	return do_mem_md(1, argc, argv);
}

static int shell_cmd_mww(int argc, char *argv[])
{
	return do_mem_mw(4, argc, argv);
}

static int shell_cmd_mwh(int argc, char *argv[])
{
	return do_mem_mw(2, argc, argv);
}

static int shell_cmd_mwb(int argc, char *argv[])
{
	return do_mem_mw(1, argc, argv);
}
#endif	/* CONFIG_CMD_MEMORY */

#if defined(CONFIG_KALLSYMS)
static int shell_cmd_symbol(int argc, char *argv[])
{
	unsigned long addr;

	if (argc < 2)
		return -EINVAL;

	addr = strtoul(argv[1], NULL, 16);

	print_ip_sym(addr);

	return 0;
}
#endif

#if defined(CONFIG_CMD_FLASH)
#include <flash.h>
static int shell_cmd_flash(int argc, char *argv[])
{
	struct device *flash_dev;
	unsigned int flash_addr;
	int length, chunk_len, remaining;
	unsigned char buf[256];
	unsigned char writeval;

	if (argc < 3) {
		printk("usage:\n");
		printk("  flash devname erase address length\n");
		printk("  flash devname read address length\n");
		printk("  flash devname write address value [,length]\n");
		printk("  flash devname info\n");
		return 0;
	}

	flash_dev = device_get_binding(argv[1]);
	if (!flash_dev) {
		printk("cannot found device \'%s\'\n", argv[1]);
		return -ENODEV;
	}

	if (!strcmp(argv[2], "erase")) {
		if (argc < 5)
			return -EINVAL;
		flash_addr = strtoul(argv[3], NULL, 16);
		length = strtoul(argv[4], NULL, 16);
		flash_erase(flash_dev, flash_addr, length);
	} else if (!strcmp(argv[2], "read")) {
		if (argc < 5)
			return -EINVAL;

		flash_addr = strtoul(argv[3], NULL, 16);
		length = strtoul(argv[4], NULL, 16);
        if (length <= 0 || length >= SHELL_MAX_MEM_COUNT) {
		    printk("Invalid length: %d\n", length);
            return -EINVAL;
        }

		remaining = length;
		while (remaining > 0) {
			chunk_len = min(remaining, 256);

			flash_read(flash_dev, flash_addr, buf, chunk_len);
			print_buffer(buf, 1, chunk_len, 16, flash_addr);

			flash_addr += chunk_len;
			remaining -= chunk_len;
		}

	} else if (!strcmp(argv[2], "write")) {
		if (argc < 5)
			return -EINVAL;

		flash_addr = strtoul(argv[3], NULL, 16);
		writeval = strtoul(argv[4], NULL, 16);
		memset(buf, writeval, 256);
		if (argc > 5)
			length = strtoul(argv[5], NULL, 16);
		else
			length = 1;

        if (length <= 0 || length >= SHELL_MAX_MEM_COUNT) {
		    printk("Invalid length: %d\n", length);
            return -EINVAL;
        }
		remaining = length;
		while (remaining > 0) {
			chunk_len = min(remaining, 256);

			flash_write(flash_dev, flash_addr, buf, chunk_len);

			flash_addr += chunk_len;
			remaining -= chunk_len;
		}
	} else if (!strcmp(argv[2], "info")) {
		/* TODO */
	} else {
		printk("invalid sub cmd");
		return -EINVAL;
	}

	return 0;
}
#endif


#if defined(CONFIG_IRQ_STAT)
#include <sw_isr_table.h>
#include <kallsyms.h>
#if defined(CONFIG_MIPS)
#include <mips32_cp0.h>
#endif
#if defined(CONFIG_CSKY)
#include <csi_core.h>
#endif

void dump_irqstat(void)
{
	struct _isr_table_entry *ite;
	int i;

	printk("IRQ statistics:\n");
	printk("irq  prio    count        max_time (us)        total (us) isr\n");

	for (i = 0; i < IRQ_TABLE_SIZE; i++) {
		ite = &_sw_isr_table[i];

		if (ite->isr != _irq_spurious) {
		    if(irq_is_enabled(i)){
                printk("%2d(*) %2d   %10d", i, NVIC_GetPriority(i), ite->irq_cnt);
		    }else{
                printk("%2d    %2d   %10d", i, NVIC_GetPriority(i), ite->irq_cnt);
		    }

#if defined(CONFIG_MIPS) && defined(CONFIG_SOC_CPU_CLK_FREQ)
			printk("    %10d",
			       ite->max_irq_cycles / (CONFIG_SOC_CPU_CLK_FREQ / 1000 / 2));
#else
			printk("    %10d",
			       SYS_CLOCK_HW_CYCLES_TO_NS(ite->max_irq_cycles) / 1000);

			printk("    %10d",
			       ite->irq_total_us);
#endif

#ifdef CONFIG_KALLSYMS
			print_symbol("            %s", (unsigned long)ite->isr);
#endif
			printk("\n");
		}
	}
}

static int shell_cmd_irqstat(int argc, char *argv[])
{
	struct _isr_table_entry *ite;
	int i;
	unsigned int key;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

    dump_irqstat();

	if (argc >= 2 && !strncmp(argv[1], "clear", strlen(argv[1]))) {
		key = irq_lock();

#ifdef CONFIG_MPU_MONITOR_RAMFUNC_WRITE
        mpu_disable_region(CONFIG_MPU_MONITOR_RAMFUNC_WRITE_INDEX);
#endif
		/* clear irq statistics */
		for (i = 0; i < IRQ_TABLE_SIZE; i++) {
			ite = &_sw_isr_table[i];

			if (ite->isr != _irq_spurious) {
				ite->irq_cnt = 0;
				ite->max_irq_cycles = 0;
			}
		}

#ifdef CONFIG_MPU_MONITOR_RAMFUNC_WRITE
        mpu_enable_region(CONFIG_MPU_MONITOR_RAMFUNC_WRITE_INDEX);
#endif
		irq_unlock(key);
	}

	return 0;
}
#endif /* CONFIG_IRQ_STAT */

#if defined(CONFIG_NVRAM_CONFIG)
#include <nvram_config.h>

static int shell_cmd_nvram(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	nvram_config_dump();

	return 0;
}
#endif /* CONFIG_NVRAM_CONFIG */

#if defined(CONFIG_STACK_BACKTRACE)
#include <stack_backtrace.h>

static int shell_dumpstack(int argc, char *argv[])
{
	show_all_threads_stack();
	return 0;
}
#endif /* CONFIG_STACK_BACKTRACE */

#ifdef CONFIG_CPU_LOAD_STAT
#include <cpuload_stat.h>
#endif

#ifdef CONFIG_CPU_LOAD_STAT
/*
 * cmd: cpuload
 *   start
 *   stop
 */
static int shell_cmd_cpuload(int argc, char *argv[])
{
	int interval_ms;
	int len = strlen(argv[1]);

	if (!strncmp(argv[1], "start", len)) {
		if (argc > 2)
			interval_ms = strtoul(argv[2], NULL, 0);
		else
			interval_ms = 2000;	/* default interval: 2s */

		printk("Start cpu load statistic, interval %d ms\n",
			interval_ms);

		cpuload_stat_start(interval_ms);

	} else if (!strncmp(argv[1], "stop", len)) {
		printk("Stop cpu load statistic\n");
		cpuload_stat_stop();
	} else {
		printk("usage:\n");
		printk("  cpuload start\n");
		printk("  cpuload stop\n");

		return -EINVAL;
	}

	return 0;
}
#endif	/* CONFIG_CPU_LOAD_STAT */

#ifdef CONFIG_CPU_LOAD_DEBUG
#include <cpuload_stat.h>

static int shell_cmd_threadblock(int argc, char *argv[])
{
	int interval_ms;
	int len = strlen(argv[1]);
	int prio = strtoul(argv[2], NULL, 0);

	if (!strncmp(argv[1], "start", len)) {
		if (argc > 3)
			interval_ms = strtoul(argv[3], NULL, 0);
		else
			interval_ms = 100;	/* default interval: 100ms */

		printk("Start cpu thread block statistic, interval %d ms\n",
			interval_ms);

		thread_block_stat_start(prio, interval_ms);

	} else if (!strncmp(argv[1], "stop", len)) {
		printk("Stop cpu thread block statistic\n");
		thread_block_stat_stop();
	} else {
		printk("usage:\n");
		printk("  threadblock start [prio][block_ms]\n");
		printk("  threadblock stop\n");

		return -EINVAL;
	}

	return 0;
}

#endif

#ifdef CONFIG_CPU_TASK_SWITCH_STAT
static int shell_cmd_task_switch_stat(int argc, char *argv[])
{
	task_switch_trace_show_all();

	return 0;
}
#endif


#if defined(CONFIG_SOC_SPICACHE_PROFILE)

#include <spicache.h>

static struct spicache_profile profile_data;

/*
 * cmd: spicache_profile
 *   start start_addr end_addr
 *   stop
 */
static int shell_cmd_spicache_profile(int argc, char *argv[])
{
	struct spicache_profile *profile;
	int len = strlen(argv[1]);

	profile = &profile_data;
	if (!strncmp(argv[1], "start", len)) {
		if (argc < 5)
			return -EINVAL;

		profile->start_addr = strtoul(argv[2], NULL, 0);
		profile->end_addr = strtoul(argv[3], NULL, 0);
		profile->spi_id = strtoul(argv[4], NULL, 0);

		printk("Start profile: addr range %08x ~ %08x\n",
			profile->start_addr, profile->end_addr);

		spicache_profile_start(profile);

	} else if (!strncmp(argv[1], "stop", len)) {
		printk("Stop profile\n");
		spicache_profile_stop(profile);
		spicache_profile_dump(profile);
	} else {
		printk("usage:\n");
		printk("  spicache_profile start start_addr end_addr\n");
		printk("  spicache_profile stop\n");

		return -EINVAL;
	}

	return 0;
}
#endif	/* CONFIG_SOC_SPICACHE_PROFILE */

#if defined(CONFIG_SYS_LOG)
static int shell_cmd_set_log_level(int argc, char *argv[])
{
	if(argv[1] != NULL) {
		int level = atoi(argv[1]);

		printk("new log level: %d\n",level);
		syslog_set_log_level(level);
	}

	return 0;
}
#endif

#if defined(CONFIG_CMD_REBOOT)
#include <soc.h>
static int shell_cmd_enter_adfu(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	printk("reboot to ADFU mode\n");
	sys_pm_reboot(REBOOT_TYPE_GOTO_ADFU);
	return 0;
}

static int shell_cmd_reboot(int argc, char *argv[])
{
	int type;

	/* default boot type */
	type = REBOOT_TYPE_GOTO_WIFISYS;

	if (argc >= 2) {
		if (strcmp("adfu", argv[1]) == 0)
			type = REBOOT_TYPE_GOTO_ADFU;
#ifdef CONFIG_SYSTEM_RECOVERY
		else if (strcmp("recovery", argv[1]) == 0) {
			system_recovery_reboot_recovery();
		}
#endif
	}

	printk("reboot type: 0x%x (use: reboot adfu)\n", type);
	sys_pm_reboot(type);

	return 0;
}
#endif

#if defined(CONFIG_CMD_POWEROFF)
#include <soc.h>
static int shell_cmd_poweroff(int argc, char *argv[])
{

	printk("system power off\n");
	sys_pm_poweroff();

	return 0;
}
#endif

#ifdef CONFIG_MPU_MONITOR_USER_DATA
extern int mpu_user_data_monitor(unsigned int start_addr, unsigned int end_addr);
static int shell_cmd_set_mpu_region(int argc, char *argv[])
{

	if(argv[1] != NULL && argv[2] != NULL) {
		int start_addr = atoi(argv[1]);
		int end_addr = atoi(argv[2]);

		printk("set mpu region: %x:%x\n", start_addr, end_addr);

		mpu_user_data_monitor(start_addr, end_addr);
	}

	return 0;
}
#endif

#ifdef CONFIG_TRACE_PERF_ENABLE
extern void trace_profile_collect(void);
static int shell_cmd_trace_profile_collect(int argc, char *argv[])
{
	trace_profile_collect();
	return 0;
}
#endif


extern void dump_sys_meminfo(uint32_t dump_detail);
static int shell_cmd_printk_meminfo(int argc, char *argv[])
{
	//dump_sys_meminfo(true);
	return 0;
}


#if defined(CONFIG_CMD_JTAG)
#include <soc.h>
static int shell_cmd_jtag(int argc, char *argv[])
{
	int group = 0;

	if (argc > 2)
		group = atoi(argv[2]);

	if (!strcmp(argv[1], "cpu") || !strcmp(argv[1], "cpu0")) {
		printk("enable cpu0 jtag group %d\n", group);
		soc_debug_enable_jtag(SOC_JTAG_CPU0, group);
	} else if (!strcmp(argv[1], "dsp")) {
		printk("enable dsp jtag group %d\n", group);
		soc_debug_enable_jtag(SOC_JTAG_DSP, group);
	} else {
		printk("unknow cpu name\n");
	}

	return 0;
}
#endif

#if defined(CONFIG_BT_CSB)
extern u32_t HWradio_Get_Radio_Register(u16_t reg);
extern void HWradio_Set_Radio_Register(u16_t reg, u16_t val);
extern void con_set_fix_tx_power_value(u16_t tx_power_value);

static int shell_cmd_serread(int argc, char *argv[])
{
	u16_t addr;
	int count;

	if (argc < 2)
		return -EINVAL;

	addr = strtoul(argv[1], NULL, 16);

	if (argc == 3)
		count = strtoul(argv[2], NULL, 16);
	else
		count = 1;

    if (count <= 0 || count >= SHELL_MAX_MEM_COUNT) {
        printk("Invalid count: %d\n", count);
		return -EINVAL;
    }

	for(int i = 0; i < count; i++, addr++)
	{
		if(i % 8 == 0) {
			printk("\n%04x:", addr);
        }
		printk(" %04x", HWradio_Get_Radio_Register(addr));
	}
	printk("\n");

	return 0;
}

static int shell_cmd_serwrite(int argc, char *argv[])
{
	u16_t addr, org, val;

	if (argc != 3)
		return -EINVAL;

	addr = strtoul(argv[1], NULL, 16);
	val = strtoul(argv[2], NULL, 16);

	org = HWradio_Get_Radio_Register(addr);
	HWradio_Set_Radio_Register(addr, val);
	val = HWradio_Get_Radio_Register(addr);

	printk("\n%04x: %04x -> %04x\n", addr, HWradio_Get_Radio_Register(addr), val);

	return 0;
}

static int shell_cmd_SetRfPower(int argc, char *argv[])
{
	u16_t val;

	if (argc != 2)
		return -EINVAL;

	val = strtoul(argv[1], NULL, 16);

	con_set_fix_tx_power_value(val);

	printk("Set Rf Power to %04x\n", val);

	return 0;
}

extern void CSB_Set_Channel(u8_t ch);

static int shell_cmd_SetChannel(int argc, char *argv[])
{
	u16_t val;

	if (argc != 2)
		return -EINVAL;

	val = strtoul(argv[1], NULL, 10);

	CSB_Set_Channel(val);

	printk("Set Channel to %d\n", val);

	return 0;
}

extern void con_gpio_debug_enable(int enable, unsigned int valid_debug_signal, unsigned char *debug_pin);

static int shell_cmd_con_debug(int argc, char *argv[])
{
	int enable, i;
	unsigned int valid_debug_signal;
	unsigned int debug_pin_u32_0, debug_pin_u32_1;
	unsigned char debug_pin[8];

	if (argc != 5 && argc != 2)
	{
		printk("usage: con_debug enable\n");
		printk("usage: con_debug enable valid_debug_signal debug_pin3-0 debug_pin7-4\n");
		return -EINVAL;
	}

	enable = strtol(argv[1], NULL, 16);

    if(argc == 5)
    {
		valid_debug_signal = strtoul(argv[2], NULL, 16);
		debug_pin_u32_0 = strtoul(argv[3], NULL, 16);
		debug_pin_u32_1 = strtoul(argv[4], NULL, 16);
    }
    else
    {
        valid_debug_signal = 0x1f;
        debug_pin_u32_0 = 0x0a0b0d08;
        debug_pin_u32_1 = 0x00000e0c;
    }

	for(i = 0; i < 4; i++)
	{
		debug_pin[i] = debug_pin_u32_0 & 0xff;
		debug_pin_u32_0 >>= 8;
	}
	for(i = 4; i < 8; i++)
	{
		debug_pin[i] = debug_pin_u32_1 & 0xff;
		debug_pin_u32_1 >>= 8;
	}

	con_gpio_debug_enable(enable, valid_debug_signal, debug_pin);

	printk("%s bt gpio debug with 0x%x, [%d,%d,%d,%d,%d,%d,%d,%d]\n"
		, enable == 1 ? "enable" : "disable"
		, valid_debug_signal
		, debug_pin[0], debug_pin[1], debug_pin[2], debug_pin[3], debug_pin[4], debug_pin[5], debug_pin[6], debug_pin[7]);

	return 0;
}


#endif

#if defined(CONFIG_RTC)
#include <rtc.h>

static void rtc_help(void)
{
	printk("usage:\n");
	printk("Get the current rtc time e.g. rtc\n");
	printk("Set the rtc time e.g. rtc set 2019-09-11 18:14:54\n");
}

static void show_rtc_time(void)
{
	struct rtc_time tm;
	struct device *dev = device_get_binding(CONFIG_RTC_0_NAME);
	rtc_get_time(dev, &tm);
	print_rtc_time(&tm);
}

static void ymdstring_to_tm(const char *timestr, struct rtc_time *tm)
{
	char *p;
	const char *split = "-";

	if (!timestr || !tm)
		return ;

	p = strtok((char *)timestr, split);
	if (!p)
		return;

	tm->tm_year = strtoul(p, NULL, 10);
	tm->tm_year -= 1900;

	p = strtok(NULL, split);
	if (!p)
		return;

	tm->tm_mon = strtoul(p, NULL, 10);
	tm->tm_mon -= 1;

	p = strtok(NULL, split);
	if (!p)
		return;

	tm->tm_mday = strtoul(p, NULL, 10);
}

static void hmsstring_to_tm(const char *timestr, struct rtc_time *tm)
{
	char *p;
	const char *split = ":";

	if (!timestr || !tm)
		return ;

	p = strtok((char *)timestr, split);
	if (!p)
		return;

	tm->tm_hour = strtoul(p, NULL, 10);

	p = strtok(NULL, split);
	if (!p)
		return;

	tm->tm_min = strtoul(p, NULL, 10);

	p = strtok(NULL, split);
	if (!p)
		return;

	tm->tm_sec = strtoul(p, NULL, 10);
}
static int shell_cmd_alarm(int argc, char *argv[])
{
	struct device *dev = device_get_binding(CONFIG_RTC_0_NAME);
	struct rtc_alarm_status sts = {0};
	if (!dev) {
		printk("Failed to get the RTC device\n");
		return -EFAULT;
	}

	rtc_get_alarm(dev, &sts);
	printk("alarm is_on=%d\n", sts.is_on);
	print_rtc_time(&sts.alarm_time);
	return 0;

}
static int shell_cmd_rtc(int argc, char *argv[])
{
	struct rtc_time tm = {0};
	struct device *dev = device_get_binding(CONFIG_RTC_0_NAME);
	if (!dev) {
		printk("Failed to get the RTC device\n");
		return -EFAULT;
	}

	if (!strcmp(argv[1], "help")) {
		rtc_help();
	} else if (!strcmp(argv[1], "set")) {
		if (argc != 4) {
			rtc_help();
			return -EPERM;
		}
		ymdstring_to_tm(argv[2], &tm);
		hmsstring_to_tm(argv[3], &tm);
		print_rtc_time(&tm);
		if (rtc_set_time(dev, &tm))
			printk("Failed to set rtc time\n");
		else
			printk("rtc set time successfully\n");
	} else {
		show_rtc_time();
	}

	return 0;
}
#endif

#if defined(CONFIG_SYS_SOC_UUID)
int uuid_sign_msg_write(int is_read);
static int shell_cmd_sign(int argc, char *argv[])
{
	if (argc != 1 && argc != 2)	{
		printk("usage: sign  [write] \n");
		return -EINVAL;
	}
	if(argc == 1) {
		 uuid_sign_msg_write(1);
	} else{
		 uuid_sign_msg_write(0);
	}
	return 0;
}
#endif


#if defined(CONFIG_DEBUG_RAMDUMP)
#include <debug/ramdump.h>

static void ramdump_print_data(void)
{
	int i;

	printk("first ramdump...\n");

	ramdump_print();

	for(i = 0; i < 500; i++){
		k_busy_wait(1000);
	}

	printk("second ramdump...\n");

	ramdump_print();
}

static int shell_cmd_ramdump(int argc, char *argv[])
{
	if (argc != 2) {
		printk("usage: ramdump save/dump\n");
		return -1;
	}
	if (!strcmp(argv[1], "save")) {
		ramdump_save();
	}else if (!strcmp(argv[1], "dump")){
		ramdump_dump();
	}else if (!strcmp(argv[1], "print")){
		ramdump_print_data();
	}
	else{
		printk("usage: ramdump save/dump\n");
	}
	return 0;
}
#endif

#if defined(CONFIG_DISABLE_IRQ_STAT)
void sys_irq_stat_dump(void);
static int shell_cmd_irq_stat_dump(int argc, char *argv[])
{
	sys_irq_stat_dump();
	return 0;
}

#endif



const struct shell_cmd kernel_commands[] = {
	{ "version", shell_cmd_version, "show kernel version" },
	{ "uptime", shell_cmd_uptime, "show system uptime in milliseconds" },
	{ "cycles", shell_cmd_cycles, "show system hardware cycles" },
#if defined(CONFIG_THREAD_MONITOR)
	{ "tasks", shell_cmd_tasks, "show running tasks" },
#endif
#if defined(CONFIG_INIT_STACKS)
	{ "stacks", shell_cmd_stack, "show system stacks" },
#if defined(CONFIG_THREAD_STACK_INFO)
	{ "stacks_usage", shell_cmd_stacks_usage, "show all stacks usage" },
#endif
#endif
#if defined(CONFIG_CMD_MEMORY)
	{ "mdw", shell_cmd_mdw, "display memory by word: mdw address [,count]" },
	{ "mdh", shell_cmd_mdh, "display memory by half-word: mdh address [,count]" },
	{ "mdb", shell_cmd_mdb, "display memory by byte: mdb address [,count]" },

	{ "mww", shell_cmd_mww, "memory write (fill) by word: mww address value [,count]" },
	{ "mwh", shell_cmd_mwh, "memory write (fill) by half-word: mwh address value [,count]" },
	{ "mwb", shell_cmd_mwb, "memory write (fill) by byte: mwb address value [,count]" },
#endif

#if defined(CONFIG_KALLSYMS)
	{ "symbol", shell_cmd_symbol, "display symbol name by address: symbol address" },
#endif

#if defined(CONFIG_CMD_FLASH)
	{ "flash", shell_cmd_flash, "flash devname erase/read/write address (value) length" },
#endif

#if defined(CONFIG_IRQ_STAT)
	{ "irqstat", shell_cmd_irqstat, "show irq statistics: irqstat [clear]" },
#endif

#if defined(CONFIG_NVRAM_CONFIG)
	{ "nvram", shell_cmd_nvram, "show nvram data" },
#endif

#if defined(CONFIG_STACK_BACKTRACE)
	{ "dumpstack", shell_dumpstack, "dump all thread stack" },
#endif /* CONFIG_STACK_BACKTRACE */

#if defined(CONFIG_CPU_LOAD_STAT)
	{ "cpuload", shell_cmd_cpuload, "show cpu load statistic preriodically" },
#endif

#if defined(CONFIG_SOC_SPICACHE_PROFILE)
	{ "spicache_profile", shell_cmd_spicache_profile, "profile spicache hit rate" },
#endif

#if defined(CONFIG_SYS_LOG)
	{ "log_level", shell_cmd_set_log_level, "set the log level" },
#endif

#if defined(CONFIG_CMD_REBOOT)
	{ "adfu", shell_cmd_enter_adfu, "reboot into ADFU upgrade mode" },
	{ "reboot", shell_cmd_reboot, "reboot system (reboot adfu)" },
#endif

#if defined(CONFIG_CMD_POWEROFF)
	{ "poweroff", shell_cmd_poweroff, "power off system" },
#endif

#if defined(CONFIG_MPU_MONITOR_USER_DATA)
    { "setmpu", shell_cmd_set_mpu_region, "set mpu protect region" },
#endif

#if defined(CONFIG_TRACE_PERF_ENABLE)
    { "trace_profile", shell_cmd_trace_profile_collect, "print trace function profile" },
#endif

#if defined(CONFIG_CPU_LOAD_DEBUG)
    { "threadblock", shell_cmd_threadblock, "thread block time statistic" },
#endif

#if defined(CONFIG_CPU_TASK_SWITCH_STAT)
    { "taskswitchstat", shell_cmd_task_switch_stat, "task switch time statistic" },
#endif

    { "meminfo", shell_cmd_printk_meminfo, "sdk heap memory statistic" },


#if defined(CONFIG_CMD_JTAG)
    { "jtag", shell_cmd_jtag, "enable jtag: jtag name(cpu/dsp) [group]" },
#endif

#if defined(CONFIG_BT_CSB)
	{ "serread", shell_cmd_serread, "display ser reg: serread address [,count]" },
	{ "serwrite", shell_cmd_serwrite, "ser write: serwrite address value" },
	{ "setrfpower", shell_cmd_SetRfPower, "set rf power: setrfpower value" },
	{ "setch", shell_cmd_SetChannel, "set rf power: setch value" },
	{ "con_debug", shell_cmd_con_debug, "enable bt gpio debug: con_debug 1. disable: con_debug 0" },
#endif

#if defined(CONFIG_RTC)
	{ "rtc", shell_cmd_rtc, "rtc get/set current time"},
	{ "alarm", shell_cmd_alarm, "rtc get alarm time"},
#endif

#if defined(CONFIG_DEBUG_RAMDUMP)
	{ "ramdump", shell_cmd_ramdump, "ramdump save/dump"},
#endif

#if defined(CONFIG_SYS_SOC_UUID)
	{ "sign", shell_cmd_sign, "sign msg"},
#endif
#if defined(CONFIG_DISABLE_IRQ_STAT)
	{ "irqdump", shell_cmd_irq_stat_dump, "irq stat dump"},
#endif

	{ NULL, NULL, NULL }
};


SHELL_REGISTER(SHELL_KERNEL, kernel_commands);
