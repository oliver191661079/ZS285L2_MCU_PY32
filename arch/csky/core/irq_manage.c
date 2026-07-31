/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 *
 * Interrupt management: enabling/disabling and dynamic ISR
 * connecting/replacing.  SW_ISR_TABLE_DYNAMIC has to be enabled for
 * connecting ISRs at runtime.
 */

#include <kernel.h>
#include <arch/cpu.h>
#include <misc/__assert.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <sw_isr_table.h>
#include <irq.h>
#include <kernel_structs.h>
#include <logging/kernel_event_logger.h>
#include <csi_core.h>
#include <mpu_acts.h>
#include <soc.h>

#define SYS_LOG_DOMAIN "irq_mgr"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_DEFAULT_LEVEL
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_IRQ_MANAGE

extern void _irq_do_offload(void);

extern struct _k_thread_stack_element _interrupt_stack[];


/**
 *
 * @brief Enable an interrupt line
 *
 * Enable the interrupt. After this call, the CPU will receive interrupts for
 * the specified <irq>.
 *
 * @return N/A
 */
void _arch_irq_enable(unsigned int irq)
{
	unsigned int key;

	key = irq_lock();

	csi_vic_enable_irq(irq);
	csi_vic_set_wakeup_irq(irq);

	irq_unlock(key);
}

/**
 *
 * @brief Disable an interrupt line
 *
 * Disable an interrupt line. After this call, the CPU will stop receiving
 * interrupts for the specified <irq>.
 *
 * @return N/A
 */
void _arch_irq_disable(unsigned int irq)
{
	unsigned int key;

	key = irq_lock();

	csi_vic_disable_irq(irq);
	csi_vic_clear_wakeup_irq(irq);

	irq_unlock(key);
}

/**
 * @brief Return IRQ enable state
 *
 * @param irq IRQ line
 * @return interrupt enable state, true or false
 */
int _arch_irq_is_enabled(unsigned int irq)
{
	return csi_vic_get_enabled_irq(irq);
}

/**
 * @brief Return IRQ pending state
 *
 * @param irq IRQ line
 * @return irq pending state, true or false
 */
int _arch_irq_get_pending(unsigned int irq)
{
	return csi_vic_get_pending_irq(irq);
}

/**
 * @internal
 *
 * @brief Set an interrupt's priority
 *
 * The priority is verified if ASSERT_ON is enabled. The maximum number
 * of priority levels is a little complex, as there are some hardware
 * priority levels which are reserved: three for various types of exceptions,
 * and possibly one additional to support zero latency interrupts.
 *
 * @return N/A
 */
void _irq_priority_set(unsigned int irq, unsigned int prio, uint32_t flags)
{
	unsigned int key;

	if (prio >= 4) {
		return;
	}

	if ((prio == 0) && (irq != IRQ_ID_BT_BASEBAND && irq != IRQ_ID_BT_TWS)){
        printk("\n\n!!!Warning: set irq %d priority %d\n\n", irq, prio);
	}

	key = irq_lock();

	csi_vic_set_prio(irq, prio);

	irq_unlock(key);
}

/**
 *
 * ebrief Spurious interrupt handler
 *
 * Installed in all dynamic interrupt slots at boot time. Throws an error if
 * called.
 *
 * See __reserved().
 *
 * @return N/A
 */
void _irq_spurious(void *unused)
{
	ARG_UNUSED(unused);
	__asm__("bkpt");
}

/* FIXME: IRQ direct inline functions have to be placed here and not in
 * arch/cpu.h as inline functions due to nasty circular dependency between
 * arch/cpu.h and kernel_structs.h; the inline functions typically need to
 * perform operations on _kernel.  For now, leave as regular functions, a
 * future iteration will resolve this.
 * We have a similar issue with the k_event_logger functions.
 *
 * See https://jira.zephyrproject.org/browse/ZEP-1595
 */

#ifdef CONFIG_SYS_POWER_MANAGEMENT
void _arch_isr_direct_pm(void)
{
}
#endif

#if defined(CONFIG_KERNEL_EVENT_LOGGER_SLEEP) || \
	defined(CONFIG_KERNEL_EVENT_LOGGER_INTERRUPT)
void _arch_isr_direct_header(void)
{
	_sys_k_event_logger_interrupt();
	_sys_k_event_logger_exit_sleep();
}
#endif

__ramfunc int _arch_irq_vector_read(void)
{
    int irqnum;

	__asm__ volatile(
	"mfcr    %0, psr \n\t"
    "lsri    %0, 16  \n\t"
    "sextb   %0      \n\t"
    "subi    %0, 32  \n\t"
	:"=r"(irqnum) : :);

	return irqnum;
}

__ramfunc int _arch_exception_vector_read(void)
{
    int excnum;

	__asm__ volatile(
	"mfcr    %0, psr \n\t"
    "lsri    %0, 16  \n\t"
    "sextb   %0      \n\t"
	:"=r"(excnum) : :);

	return excnum;
}

#define RUNNING_CYCLES(end, start)	((uint32_t)((long)(end) - (long)(start)))

#if defined(CONFIG_CPU_LOAD_DEBUG) && defined(CONFIG_IRQ_STAT)

#define RUNNING_TIMES(end, start)	((uint32_t)((long)(end) - (long)(start)))
extern void idle(void *unused1, void *unused2, void *unused3);

static u32_t cpuload_isr_total_cycles;
static u32_t cpuload_isr_in_idle_cycles;

static u32_t irq_pre_total_us[IRQ_TABLE_SIZE];

void cpuload_irq_manager_curr_state(void)
{
	int i;
	unsigned int key;
	struct _isr_table_entry *ite;

	key = irq_lock();

	printk("\n(%u)total isr %d us, in idle %d us\n", k_uptime_get_32(),
			SYS_CLOCK_HW_CYCLES_TO_NS_AVG(cpuload_isr_total_cycles, 1000),
			SYS_CLOCK_HW_CYCLES_TO_NS_AVG(cpuload_isr_in_idle_cycles, 1000));

	printk("current irqs cpuload state:\n");
	for (i=0; i<IRQ_TABLE_SIZE; i++) {
		ite = &_sw_isr_table[i];
		if (ite->isr != _irq_spurious) {
			printk("irq@%d runtime %dus\n", i, RUNNING_TIMES(ite->irq_total_us, irq_pre_total_us[i]));
		}
	}

	irq_unlock(key);
}
#endif

static u32_t irq_count = 0;

/**
 * @brief Interrupt demux function
 *
 * Given a bitfield of pending interrupts, execute the appropriate handler
 *
 * @param ipending Bitfield of interrupts
 */
__ramfunc void _isr_wrapper(void)
{
#ifdef CONFIG_IRQ_STAT
	u32_t ts_irq_enter, irq_cycles;
	u32_t stop_cycle;
#ifdef CONFIG_CPU_LOAD_DEBUG
	static u32_t pre_time, curr_time;
	u32_t i, print_flag = 0;
#endif

#ifdef CONFIG_MPU_MONITOR_RAMFUNC_WRITE
	u32_t key;
	int mpu_enable;
#endif
#endif

    struct _isr_table_entry *ite;
    u32_t irqnum = _arch_irq_vector_read();

	_kernel.nested++;
	irq_count++;

#ifdef CONFIG_IRQ_OFFLOAD
	_irq_do_offload();
#endif

#ifdef CONFIG_MONITOR_IRQ_STACKSIZE
	if(csi_get_sp() - (uint32_t)_interrupt_stack < CONFIG_MONITOR_IRQ_MIN_FREESIZE){
		printk("isr stack free size too low %d\n", csi_get_sp() - (uint32_t)_interrupt_stack);
        panic(0);
	}
#endif

#ifdef CONFIG_IRQ_STAT
	ts_irq_enter = k_cycle_get_32();
#endif

	ite = &_sw_isr_table[irqnum];

	if(ite->isr){
	    ite->isr(ite->arg);
	}else{
		printk("irq@%d without isr routine\n", irqnum);
        panic(0);
	}

#ifdef CONFIG_IRQ_STAT

#ifdef CONFIG_MPU_MONITOR_RAMFUNC_WRITE
	key = irq_lock();

	mpu_enable = mpu_region_is_enable(CONFIG_MPU_MONITOR_RAMFUNC_WRITE_INDEX);
	if (mpu_enable) {
		mpu_disable_region(CONFIG_MPU_MONITOR_RAMFUNC_WRITE_INDEX);
	}
#endif

	ite->irq_cnt++;
	stop_cycle = k_cycle_get_32();
	irq_cycles = RUNNING_CYCLES(stop_cycle, ts_irq_enter);
	ite->irq_total_us += SYS_CLOCK_HW_CYCLES_TO_NS(irq_cycles) / 1000;
	if (irq_cycles > ite->max_irq_cycles)
		ite->max_irq_cycles = irq_cycles;

#ifdef CONFIG_IRQ_LATENCY_STAT
	if(irq_cycles >= CONFIG_IRQ_LATENCY_STAT_TIME_US){
		printk("irq@%d run %dus not meet latency\n", irqnum, irq_cycles / 24);
	}
#endif

#ifdef CONFIG_CPU_LOAD_DEBUG
	cpuload_isr_total_cycles += RUNNING_CYCLES(stop_cycle, ts_irq_enter);
	if (_current->keep_entry == idle) {
		cpuload_isr_in_idle_cycles += RUNNING_CYCLES(stop_cycle, ts_irq_enter);
	}

	curr_time = k_uptime_get_32();
	if ((curr_time - pre_time) > 1000) {
		pre_time = curr_time;

		for (i=0; i<IRQ_TABLE_SIZE; i++) {
			ite = &_sw_isr_table[i];

			if (ite->isr != _irq_spurious) {
				if ((i == IRQ_ID_BT_BASEBAND) &&
					(RUNNING_TIMES(ite->irq_total_us, irq_pre_total_us[i]) > 200000)) {
					printk("current irq@%d overtime run %dus\n", i, RUNNING_TIMES(ite->irq_total_us, irq_pre_total_us[i]));
					print_flag = 1;
				} else if ((i != IRQ_ID_BT_BASEBAND) &&
					(RUNNING_TIMES(ite->irq_total_us, irq_pre_total_us[i]) > 60000)) {
					printk("current irq@%d overtime run %dus\n", i, RUNNING_TIMES(ite->irq_total_us, irq_pre_total_us[i]));
					print_flag = 1;
				}

				irq_pre_total_us[i] = ite->irq_total_us;
			}
		}

		/* > (300ms)  (7310000/1000)*41 */
		/* SYS_CLOCK_HW_CYCLES_TO_NS_AVG(1000, 1000) = 41us */
		if ((cpuload_isr_total_cycles > 7310000) || print_flag) {
			printk("(%u)total isr %d us, in idle %d us\n", curr_time,
					SYS_CLOCK_HW_CYCLES_TO_NS_AVG(cpuload_isr_total_cycles, 1000),
					SYS_CLOCK_HW_CYCLES_TO_NS_AVG(cpuload_isr_in_idle_cycles, 1000));
		}
		cpuload_isr_total_cycles = 0;
		cpuload_isr_in_idle_cycles = 0;
	}
#endif

#ifdef CONFIG_MPU_MONITOR_RAMFUNC_WRITE
    if(mpu_enable){
        mpu_enable_region(CONFIG_MPU_MONITOR_RAMFUNC_WRITE_INDEX);
    }

    irq_unlock(key);
#endif

#endif

	_kernel.nested--;
}

u32_t _arch_irq_get_irq_count(void)
{
	return irq_count;
}

#ifdef CONFIG_DISABLE_IRQ_STAT

#include <stack_backtrace.h>

static u32_t disable_irq_timer;
static u8_t irq_stat_msg_on = 1;

#define IRQ_STAT_MAX_ITEM	10
struct irq_stat_dat {
	const void *caller_addr;   
	u32_t irq_dis_usec;  /* irq disable time use */
};

static struct irq_stat_dat  irq_stat_msg_dat[IRQ_STAT_MAX_ITEM];
static u8_t irq_stat_cur_num = 0;

void sys_irq_stat_dump(void)
{
	u8_t i;
	struct irq_stat_dat *pf;
	printk("irq_stat:larger than %us, num=%d\n", CONFIG_DISABLE_IRQ_US, irq_stat_cur_num);
	for(i = 0; i < irq_stat_cur_num; i++){
		pf = &irq_stat_msg_dat[i];
		printk("%d. %p disalbed irq %d us\n", i, pf->caller_addr, pf->irq_dis_usec);
	}
}

static void  sys_irq_stat_handle(u32_t use_us, const void *caller_addr)
{
	u8_t i;
	struct irq_stat_dat *pf;
	for(i = 0; i < irq_stat_cur_num; i++){
		if(irq_stat_msg_dat[i].caller_addr == caller_addr){
			pf = &irq_stat_msg_dat[i];
			if(pf->irq_dis_usec < use_us)
				pf->irq_dis_usec = use_us;	
			return;
		}
	}
	if(irq_stat_cur_num < IRQ_STAT_MAX_ITEM){
		pf = &irq_stat_msg_dat[irq_stat_cur_num];
		pf->caller_addr = caller_addr;
		pf->irq_dis_usec = use_us;
		irq_stat_cur_num++;
	}else{
		/*统计超过 IRQ_STAT_MAX_ITEM，直接打印*/
		printk("\n\n!!! %p disalbed irq %d us larger than %d us\n\n", caller_addr, use_us, CONFIG_DISABLE_IRQ_US);
		sys_irq_stat_dump();
	}
}


void sys_irq_stat_msg_ctl(bool enable)
{
	irq_stat_msg_on = enable;
}

static ALWAYS_INLINE unsigned int __arch_irq_lock(void)
{
        unsigned long flags;
        __asm__ __volatile__(
                "mfcr   %0, psr \n"
                "psrclr ie      \n"
                :"=r"(flags)
                :
                :"memory"
                );
        return flags;
}

static ALWAYS_INLINE void __arch_irq_unlock(unsigned int flags)
{
        __asm__ __volatile__(
                "mtcr   %0, psr \n"
                :
                :"r" (flags)
                :"memory"
                );
}

unsigned int _arch_irq_lock(void)
{
    u32_t flags;

    flags = __arch_irq_lock();

    if(flags & PSR_IE_Msk){
        disable_irq_timer = k_cycle_get_32();
    }

    return flags;
}

void _arch_irq_unlock(unsigned int flags)
{
    u32_t disable_irq_us;

    if(flags & PSR_IE_Msk){
        disable_irq_us = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32() - disable_irq_timer) / 1000;

        if(irq_stat_msg_on && (disable_irq_us > CONFIG_DISABLE_IRQ_US)){
			#if 0
        	printk("\n\n!!! %p disalbed irq %d.%d ms larger than %d us\n\n", __builtin_return_address(0),
        			disable_irq_us / 1000, disable_irq_us % 1000, CONFIG_DISABLE_IRQ_US);
        	dump_stack();
			#else
			sys_irq_stat_handle(disable_irq_us, __builtin_return_address(0));
			#endif
        }
    }
    __arch_irq_unlock(flags);
}



#endif

#if defined(CONFIG_SYS_IRQ_LOCK)
__ramfunc void sys_irq_lock(SYS_IRQ_FLAGS *flags)
{
	u32_t key;

	if (!_is_in_exc_exclude_tspend()) {
		k_sched_lock();
	}

	key = irq_lock();

	flags->keys[0] = VIC->ISER[0];
	flags->keys[1] = VIC->ISER[1];

	/* only enable irq0(bluetooth irq) */
#ifdef CONFIG_SYS_IRQ_LOCK_EXCEPT_BT_CON
	VIC->ICER[0] = flags->keys[0] & ~(1u << IRQ_ID_BT_BASEBAND);
#else
	VIC->ICER[0] = flags->keys[0];
#endif

	VIC->ICER[1] = flags->keys[1];

	irq_unlock(key);
}

__ramfunc void sys_irq_unlock(const SYS_IRQ_FLAGS *flags)
{
	u32_t key;

	key = irq_lock();

	VIC->ISER[0] = flags->keys[0];
	VIC->ISER[1] = flags->keys[1];

	irq_unlock(key);

	if (!_is_in_exc_exclude_tspend()) {
		k_sched_unlock();
	}
}
#else
void sys_irq_lock(SYS_IRQ_FLAGS *flags)
{
	flags->keys[0] = irq_lock();
}

void sys_irq_unlock(const SYS_IRQ_FLAGS *flags)
{
	irq_unlock(flags->keys[0]);
}
#endif
