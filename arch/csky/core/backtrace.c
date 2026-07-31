/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file peripheral clock interface for Actions SoC
 */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#include <kernel_structs.h>
#include <kallsyms.h>
#include <stack_backtrace.h>
#include "csky_backtrace.h"
#include <linker/linker-defs.h>

extern struct _k_thread_stack_element _interrupt_stack[];
extern NANO_ESF _default_esf;
extern int _arch_exception_vector_read(void);

#ifdef CONFIG_ENABLE_TRACE_EVTTOSTR
extern const char *task_evt2str(int num);
#endif

uint32_t csky_get_current_pc(void);
__asm__ (
	".text\n\t"
	".global csky_get_current_pc\n\t"
	"csky_get_current_pc:\n\t"
	"mov    r0, lr\n\t"
	"rts\n\t");

uint32_t csky_get_current_sp(void);
__asm__ (
	".text\n\t"
	".global csky_get_current_sp\n\t"
	"csky_get_current_sp:\n\t"
	"mov    r0, sp\n\t"
	"rts\n\t");

#ifndef CONFIG_KALLSYMS
static void sh_dump_mem(const char *msg, uint32_t mem_addr, int len)
{
	int i;
	uint32_t *ptr = (uint32_t *)mem_addr;
	printk("%s start=0x%08x end=0x%08x\n", msg, mem_addr, mem_addr + len);
	for(i = 0; i < len/4; i++)
	{
		printk("%08x ", ptr[i]);
		if(i % 8 == 7)
			printk("\n");
	}
	printk("\n\n");
}

static uint32_t check_mem_is_valid(u32_t addr)
{
	if(addr >= CONFIG_SRAM_BASE_ADDRESS && addr <= (CONFIG_SRAM_BASE_ADDRESS + (CONFIG_SRAM_SIZE * 1024))){
		return true;
	}else if(addr >= 0x30000 && addr <= 0x40000){
		//hardware memory
		return true;
	}else if(addr >= 0x68000000 && addr <= (0x68008000)){
		return true;
	}
#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
	else if(addr >= CONFIG_SOC_MAPPING_PSRAM_ADDR && addr <= (CONFIG_SOC_MAPPING_PSRAM_ADDR + CONFIG_SOC_MAPPING_PSRAM_SIZE)){
		return true;
	}
#endif
	else{
		return false;
	}
}

uint32_t mem_is_ram_data(uint32_t addr)
{
	if(!check_mem_is_valid(addr)){
		printk("\n mem addr 0x%x is invalid\n", addr);
		return false;
	}

	return true;

}

uint32_t get_excpetion_user_register(uint32_t index);

static uint32_t _check_sp_end_addr(uint32_t sp_cur)
{
	struct k_thread *thread_list;

	if ((sp_cur >= (uint32_t)_interrupt_stack)
		&& (sp_cur <= (uint32_t)_interrupt_stack + CONFIG_ISR_STACK_SIZE)) {
		return (uint32_t)_interrupt_stack + CONFIG_ISR_STACK_SIZE;
	}

	//if sp_cur is current excpetion sp, set sp end current thread
	if(_IsInExc() && !_IsInIsr()){
		if(sp_cur == get_excpetion_user_register(14)){
			thread_list = k_current_get();
			if(!mem_is_ram_data((uint32_t)thread_list)){
				return sp_cur;
			}
			return thread_list->stack_info.start + thread_list->stack_info.size;
		}
	}


	thread_list = ((struct k_thread *)(_kernel.threads));

	if(!mem_is_ram_data((uint32_t)thread_list)){
		return sp_cur;
	}

	while (thread_list != NULL) {
		if (sp_cur >= thread_list->stack_info.start && sp_cur <= (thread_list->stack_info.start + thread_list->stack_info.size)) {
			break;
		}

		thread_list = (struct k_thread *)(thread_list->next_thread);

		if(!mem_is_ram_data((uint32_t)thread_list)){
			thread_list = NULL;
			break;
		}
	}

	if (!thread_list) {
		//Can not get stack area, maybe stack overflows.
		printk("stack error! overflow? SP=<%08x>\n", sp_cur);
		return sp_cur + 0x100;
	} else {
		return thread_list->stack_info.start + thread_list->stack_info.size;
	}
}

static int sh_backtrace_is_funcs(uint32_t pc)
{
	if ((pc >= (unsigned long)&_image_rom_start) && (pc <= (unsigned long)&_image_text_end)) {
		return 1;
	}

	if ((pc >= (unsigned long)&_image_text_ramfunc_start) && (pc <= (unsigned long)&_image_text_ramfunc_end)) {
		return 1;
	}

	if ((pc >= (unsigned long)&__btcon_ramfunc_start) && (pc <= (unsigned long)&__btcon_ramfunc_end)) {
		return 1;
	}

	return 0;
}


static void sh_backtrace_print_elx(uint32_t addr, uint32_t sp, uint32_t bak_lr, struct __esf *esf)
{
	uint32_t where = sp;
	uint32_t pc = 0;
	uint32_t stk_end = _check_sp_end_addr(sp);

	if(sp > stk_end || !check_mem_is_valid(sp) || !check_mem_is_valid(stk_end)){
		printk("stack overflow sp<%08x> end<%08x>\n", sp, stk_end);
		return;
	}

	printk("/opt/csky-elfabiv2/bin/csky-abiv2-elf-addr2line -e zephyr.elf -a -f ");
	if(esf){
		printk("%08x %08x ", esf->epc, esf->r15);
	}else{
		printk("%08x ", addr);
	}
	while (where < stk_end) {
		pc = *(uint32_t*)where;
		if(sh_backtrace_is_funcs(pc)){
			printk("%08x ", pc);
		}

		where += 4;
	}
	printk("\n");

	sh_dump_mem("stack:", sp, stk_end - sp);
	printk("\n");
}

static void sh_backtrace_get_stack(uint32_t *sp, uint32_t *sp_len)
{
	struct k_thread *th = k_current_get();
	uint32_t stack_start, stack_sz;

	if (k_is_in_isr()) {
		*sp = csky_get_current_sp();
		stack_start = (uint32_t)_interrupt_stack;
		stack_sz = CONFIG_ISR_STACK_SIZE;
	} else {
		*sp = csky_get_current_sp();
		stack_start = (uint32_t)th->stack_info.start;
		stack_sz = th->stack_info.size;

	}

	*sp_len = (stack_start + stack_sz - *sp) / sizeof(uint32_t);

	//fix sp len maybe invalid
	if(*sp_len > 0x2000){
		*sp_len = 0x100;
	}
}

void sh_backtrace_get_funcs(uint32_t *func, uint32_t num, uint32_t skip_func_addr)
{
	uint32_t pc;
	uint32_t sp, sp_len;
	uint32_t *buf;
	uint32_t skip_func_num = 6;
	uint32_t last_sp_len = 0;
	uint32_t *last_buf;

	sh_backtrace_get_stack(&sp, &sp_len);

	buf = (uint32_t *)sp;

	while ((sp_len > 0) && (skip_func_addr > 0)) {
		pc = *buf;
		buf++;
		sp_len --;

		if (sh_backtrace_is_funcs(pc)) {
			if(pc == skip_func_addr){
				last_sp_len = (sp_len - 1);
				last_buf = (buf - 1);
				skip_func_num--;
				if(skip_func_num == 0){
					break;
				}
			}
		}

	}

	if(last_sp_len){
		sp_len = last_sp_len;
		buf = last_buf;
	}

	while ((sp_len > 0) && (num > 0)) {
		pc = *buf;
		buf ++;
		sp_len --;

		if (sh_backtrace_is_funcs(pc)) {
			*func = pc;
			func ++;
			num --;
		}
	}

	while (num > 0) {
		*func = 0;
		func ++;
		num --;
	}
}


#endif

static void __show_backtrace(struct k_thread *thread, struct __esf *esf)
{
	uint32_t addr;
	uint32_t sp;
	uint32_t bak_lr = 0;

	if (esf) {
		addr = esf->epc;

		if(esf->vec == 7){
			addr = esf->r15;
		}
		sp = esf->r14;
		/* first frame maybe doesn't have stack frame */
		bak_lr = esf->r15;
	} else {
		if (thread != NULL && thread != k_current_get()) {
			addr = thread->callee_saved.pc;
			sp = thread->callee_saved.sp;
		} else {
			addr = csky_get_current_pc();
			sp = csky_get_current_sp();
		}
	}

	TRACE_SYNC("Call Trace:\n");
#ifdef CONFIG_KALLSYMS
	csky_backtrace(addr, sp, bak_lr);
#else
	sh_backtrace_print_elx(addr, sp, bak_lr, esf);
#endif
}

void show_backtrace(struct k_thread *thread, struct __esf *esf)
{
    uint32_t param[4] = {
            (uint32_t) thread,
            (uint32_t) esf,
            (uint32_t) _interrupt_stack + CONFIG_ISR_STACK_SIZE,
            (uint32_t) __show_backtrace };

    uint32_t flags;

    flags = irq_lock();

	if ((uint32_t) &flags > (uint32_t) _interrupt_stack
			&& (uint32_t) &flags < \
			((uint32_t) _interrupt_stack + CONFIG_ISR_STACK_SIZE)) {

        __show_backtrace(thread, esf);
    }else{
        //backtrace maybe overflow in thread stack, we must switch it to interrupt stack
        //save the thread context,include sp, argument registers ...
		__asm__ __volatile (
				"	ldw	r0,	%0	\n"
				"	ldw	r1,	%1	\n"
				"	ldw	r2,	%2	\n"
				"	mov	r4,	r14	\n"
				"	mov	r14,r2	\n"
				"	ldw	r2,	%3	\n"
				"	jsr	r2		\n"
				"	mov	r14, r4	\n"
				:
				: "m" (param[0]), "m" (param[1]), "m" (param[2]), "m" (param[3])
				: "r0", "r1", "r2", "r4", "r14");
    }

    irq_unlock(flags);
}


unsigned int get_user_sp(void)
{
    uint32_t *sp_ptr = (uint32_t *)_interrupt_stack;

    return sp_ptr[CONFIG_ISR_STACK_SIZE / 4 - 2];
}


void show_thread_stack(struct k_thread *thread)
{
    struct __esf esf;

    struct irq_frame *frame;

    //if in isr, we must print irq stack and thread stack
    if(k_is_in_isr() && thread == k_current_get()){
        TRACE_SYNC("ISR: %d\n", _arch_irq_vector_read());

        show_backtrace(thread, NULL);

#ifdef CONFIG_ENABLE_TRACE_EVTTOSTR
	    TRACE_SYNC("Thread: %p %s %d %s\n", thread,
		thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread), task_evt2str(k_thread_priority_get(thread)));
#else
		TRACE_SYNC("Thread: %p %s %d 0x%x\n", thread,
		thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread), thread->base.thread_state);
#endif

		//get the thread sp value from the top of interrupt stack
		frame = (struct irq_frame *)(_interrupt_stack + CONFIG_ISR_STACK_SIZE - sizeof(struct irq_frame));

        esf.r14 = get_user_sp();
        esf.epc = frame->epc;
        esf.r15 = frame->r15;
        esf.vec = 32;

		//TRACE_SYNC("irq frame pc %x\n", frame->epc);
		//TRACE_SYNC("irq frame ra %x\n", frame->r15);
		//TRACE_SYNC("irq frame sp %x\n", esf.r14);

		show_backtrace(thread, &esf);

    }else if((_arch_exception_vector_read() > 0) && (thread == k_current_get())){
        TRACE_SYNC("exception: %d\n", _arch_exception_vector_read());

        show_backtrace(thread, NULL);

#ifdef CONFIG_ENABLE_TRACE_EVTTOSTR
	    TRACE_SYNC("Thread: %p %s %d %s\n", thread,
		thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread), task_evt2str(k_thread_priority_get(thread)));
#else
		TRACE_SYNC("Thread: %p %s %d 0x%x\n", thread,
		thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread), thread->base.thread_state);
#endif

        esf.r14 = _default_esf.r14;
        esf.epc = _default_esf.epc;
        esf.r15 = _default_esf.r15;
        esf.vec = _arch_exception_vector_read();

		show_backtrace(thread, &esf);

		//check if in interrupt exception
		if(((_default_esf.epsr >> 16) & 0xff) > 32){
			frame = (struct irq_frame *)(_interrupt_stack + CONFIG_ISR_STACK_SIZE - sizeof(struct irq_frame));
	        esf.r14 = get_user_sp();
	        esf.epc = frame->epc;
	        esf.r15 = frame->r15;
	        esf.vec = 32;

			show_backtrace(thread, &esf);
		}
    }else{
#ifdef CONFIG_ENABLE_TRACE_EVTTOSTR
    	TRACE_SYNC("Thread: %p %s %d %s\n", thread,
    		thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread), task_evt2str(k_thread_priority_get(thread)));
#else
		TRACE_SYNC("Thread: %p %s %d 0x%x\n", thread,
			thread == k_current_get() ? "*" : " ", k_thread_priority_get(thread), thread->base.thread_state);
#endif
        if(thread == k_current_get()){
            esf.r14 = csky_get_current_sp();
            esf.epc = csky_get_current_pc();
            esf.r15 = 0;
            esf.vec = 0;
            show_backtrace(thread, &esf);
        }else{
            show_backtrace(thread, NULL);
        }
    }


}

void show_all_threads_stack(void)
{
#if defined(CONFIG_THREAD_MONITOR)
	struct k_thread *thread = NULL;

	thread = (struct k_thread *)(_kernel.threads);
	while (thread != NULL) {
		if(!mem_is_ram_data((uint32_t)thread)){
			break;
		}
		show_thread_stack(thread);
		thread = (struct k_thread *)thread->next_thread;
	}
#endif
}

void dump_stack(void)
{
	show_thread_stack(k_current_get());
}
