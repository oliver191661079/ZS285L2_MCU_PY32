/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 */

#include <kernel.h>
#include <arch/cpu.h>
#include <kernel_structs.h>
#include <misc/printk.h>
#include <inttypes.h>
#include <arch/csky/arch.h>
#include <stack_backtrace.h>
#include <soc.h>
#include <csi_core.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_FATAL

extern crash_dump_t crash_dump_start[];
extern crash_dump_t crash_dump_end[];


#define list_for_each_crash_dump(a) \
	for (a = crash_dump_start; a < crash_dump_end; a++)


#ifdef CONFIG_STACK_BACKTRACE
extern void show_backtrace(struct k_thread *thread, const struct __esf *esf);
#endif

#ifdef CONFIG_MPU_EXCEPTION_DRIVEN
extern int mpu_handler(void);
#endif

extern int xspi_crc_error_exception(void);


NANO_ESF _default_esf = {
	0xdeadbaad, //r0
	0xdeadbaad, //r1
	0xdeadbaad, //r2
	0xdeadbaad, //r3
	0xdeadbaad, //r4
	0xdeadbaad, //r5
	0xdeadbaad, //r6
	0xdeadbaad, //r6
	0xdeadbaad, //r7
	0xdeadbaad, //r8
	0xdeadbaad, //r9
	0xdeadbaad, //r10
	0xdeadbaad, //r11
	0xdeadbaad, //r12
	0xdeadbaad, //r13
	0xdeadbaad, //r14
	0xdeadbaad, //r15
	0xdeadbaad, //epsr
	0xdeadbaad  //epc
};

const char exception_names[][32] = {
	"RESET PC",
	"BUS ERROR",
	"ADDRESS ERROR",
	"ZERO DIVIDE",
	"ILLEGAL INSTRUCTION",
	"PRIVILEGE VIOLATION",
	"TRACE",
	"BREAKPOINT ERROR",
	"FATAL ERROR",
	"Idly4 ERROR",
	"",
	"",
	"UNASSIGNED RESERVED HAI",
	"UNASSIGNED RESERVED FP",
	"",
	"",
	"TRAP #0",
	"TRAP #1",
	"TRAP #2",
	"TRAP #3",
	"UNASSIGNED RESERVED 20",
	"UNASSIGNED RESERVED 21",
	"UNASSIGNED RESERVED 22",
	"UNASSIGNED RESERVED 23",
	"UNASSIGNED RESERVED 24",
	"UNASSIGNED RESERVED 25",
	"UNASSIGNED RESERVED 26",
	"UNASSIGNED RESERVED 27",
	"UNASSIGNED RESERVED 28",
	"UNASSIGNED RESERVED 29",
	"UNASSIGNED RESERVED 30",
	"SYSTEM DESCRIPTION POINT",
};

static const char reg_name[][4] = {
    "r0", "r1", "r2", "r3",
    "r4", "r5", "r6", "r7",
    "r8", "r9", "r10","r11",
    "r12", "r13","sp", "ra",
};


const char separate_line[] = "----------------------------------------\r\n";

extern void trace_set_panic(void);

static void crash_dump_register(void)
{
	int i;

	u32_t *addr;

	const NANO_ESF *pEsf = &_default_esf;

    addr = (u32_t *)pEsf;
    PR_EXC("CPU Exception : %d\n", pEsf->vec);
    PR_EXC("Exception reason : %s\n", &exception_names[pEsf->vec][0]);

    for (i = 0; i < 16; i++) {
        PR_EXC("%s :%08x ", &reg_name[i][0], addr[i]);

        if ((i % 5) == 4) {
            PR_EXC("\n");
        }
    }

    PR_EXC("\n");
    PR_EXC("epsr: %8x\n", pEsf->epsr);
    PR_EXC("Current thread ID = %p\n"
        "Faulting instruction address = 0x%x\n",
        k_current_get(), pEsf->epc);

	PR_EXC("cpu error addr: %x\n", sys_read32(0xc00a0008));
}

static void crash_dump_backtrace(void)
{
	PR_EXC("Thread: %p * %d 0x%x\n", 
		_current, k_thread_priority_get(_current), _current->base.thread_state);

#ifdef CONFIG_STACK_BACKTRACE
	show_backtrace(_current, &_default_esf);
#endif

	show_all_threads_stack();
}


static u8_t sys_crash_flag;
static u8_t sys_fault_cnt;
static u8_t sys_fault_reason;
//按照CSKY calling convention文档寄存器使用约定
//a0-a3(r0-r3)用作寄存器传参，同时用作函数返回值
//s0-s7(r4-r11)用作子函数局部变量
//t0-t1(r12,r13)用作临时寄存器
//sp(r14)用作堆栈指针
//ra(r15)用作链接寄存器，存储返回地址
void _NanoFatalErrorHandler(unsigned int reason,
                                          const NANO_ESF *pEsf)
{
    crash_dump_t *crash_dump;

	sys_fault_cnt++;

	sys_fault_reason = reason;

    if(!xspi_crc_error_exception()){
        sys_write32((sys_read32(MEMORYCTL) & (~0x01030000)) | MEMORYCTL_CRCERROR_BIT, MEMORYCTL);
        printk("exception reason %d\n", reason);
		sys_fault_reason = 0xff;
        return;
    }

#ifdef CONFIG_MPU_EXCEPTION_DRIVEN
    if(sys_read32(MPUIP)){
        if(!mpu_handler()){
			sys_fault_reason = 0xfd;
            return;
        }
    }
#endif

#ifndef CONFIG_CPU_REBOOT_IF_EXCEPTION
	soc_watchdog_stop();
#endif

	/* 为避免重复进入异常，只在第一次进入异常时做dump处理，第二次只打印GPR寄存器数值 */
	if(!sys_crash_flag){
		sys_crash_flag = true;

		trace_set_panic();

	    list_for_each_crash_dump(crash_dump){
	        PR_EXC("%s", separate_line);
	        crash_dump->dump();
	    }
	}else{
		crash_dump_register();
	}

	_SysFatalErrorHandler(reason, pEsf);
}

void trap_c(const NANO_ESF *pEsf)
{
	_NanoFatalErrorHandler(pEsf->vec, pEsf);
}

void panic(const char *err_msg)
{
	trace_set_panic();

	irq_lock();

	PR_EXC("Oops: system panic:\n");
	if (err_msg) {
		PR_EXC("%s\n", err_msg);
	}

    __BKPT();

    __asm__ __volatile ("nop" ::: "memory");
}

CRASH_DUMP_REGISTER(dump_register, 0) =
{
    .dump = crash_dump_register,
};

CRASH_DUMP_REGISTER(dump_backtrace, 90) =
{
    .dump = crash_dump_backtrace,
};

uint32_t get_excpetion_user_register(uint32_t index)
{
    uint32_t *ptr = (uint32_t *)&_default_esf;
    return ptr[index];
}

uint32_t get_exception_esf_info(void)
{
	return (uint32_t)&_default_esf;
}
