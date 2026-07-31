/********************************************************************************
 *                            USDK(ZS283A_clean)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-25-上午10:02:02             1.0             build this file
 ********************************************************************************/
/*!
 * \file     mpu_acts.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-25-上午10:02:02
 *******************************************************************************/
#include <kernel.h>
#include <init.h>
#include <soc.h>
#include <mpu_acts.h>
#include <linker/linker-defs.h>
#include <os_common_api.h>
#include <stack_backtrace.h>

#define SYS_LOG_DOMAIN "mpu"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_DEFAULT_LEVEL
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_MPU_ACTS

#ifdef CONFIG_MPU_ACTS

extern unsigned int get_user_sp(void);
int mpu_ignore_invalid_dma_fault(void);
static void mpu_clear_all_pending(void)
{
    sys_write32(0xffffffff, MPUIP);
}

void mpu_protect_exit(void)
{
    int i;
    mpu_protect_disable();
    for(i = 0; i < CONFIG_MPU_ACTS_MAX_INDEX; i++){
        sys_write32(0, MPUCTL0 + i * CONFIG_MPU_ACTS_MAX_INDEX);
    }
    mpu_clear_all_pending();
}

void mpu_set_address(uint32_t start, uint32_t end, uint32_t flag, uint32_t index)
{
    mpu_base_register_t *base_register = (mpu_base_register_t *)MPUCTL0;

    base_register += index;

    if(flag == MPU_DSP_WRITE){
        start = mcu_to_mpu_dsp_addr(start);
        end = mcu_to_mpu_dsp_addr(end);
    }

    base_register->MPUBASE = start,
    base_register->MPUEND = end;
    base_register->MPUCTL = ((flag) << 1);
}

void mpu_protect_init(void)
{

#ifdef CONFIG_MPU_MONITOR_CACHECODE_WRITE
    /* CACHE地址均是不可写的 */
    mpu_set_address((unsigned int)&_image_rom_start, (unsigned int)&_image_rom_end - 1, \
        (MPU_CPU_WRITE | MPU_DMA_WRITE), CONFIG_MPU_MONITOR_CACHECODE_WRITE_INDEX);

    mpu_enable_region(CONFIG_MPU_MONITOR_CACHECODE_WRITE_INDEX);
#endif

#ifdef CONFIG_MPU_MONITOR_RAMFUNC_WRITE
    /* TEXT地址均是不可写的 */
    mpu_set_address((unsigned int)&__mpu_ram_protect_start, (unsigned int)&__mpu_ram_protect_end - 1, \
        (MPU_CPU_WRITE | MPU_DMA_WRITE), CONFIG_MPU_MONITOR_RAMFUNC_WRITE_INDEX);

    mpu_enable_region(CONFIG_MPU_MONITOR_RAMFUNC_WRITE_INDEX);
#endif



#ifdef CONFIG_MPU_MONITOR_DSP_WRITE_MCU_MEMORY
#ifdef CONFIG_SOC_SERIES_ANDES
    /*除dsp ram外，其它的地址dsp均不可写*/
    mpu_set_address((u32_t)&__data_ram_start, \
        (u32_t)0x60000 - 1, MPU_DSP_WRITE, CONFIG_MPU_MONITOR_DSP_WRITE_MCU_MEMORY_INDEX);
#else
    mpu_set_address((u32_t)&__mpu_ram_protect_start, \
        (u32_t)&__mpu_ram_protect_end - 1, MPU_DSP_WRITE, CONFIG_MPU_MONITOR_DSP_WRITE_MCU_MEMORY_INDEX);
#endif
    mpu_enable_region(CONFIG_MPU_MONITOR_DSP_WRITE_MCU_MEMORY_INDEX);

#endif

#ifdef CONFIG_FPGA_SIM
	mpu_set_address(0x40000, 0x60000 - 1, \
		(MPU_CPU_WRITE | MPU_DMA_WRITE), CONFIG_MPU_MONITOR_RAMFUNC_WRITE_INDEX);

	mpu_enable_region(CONFIG_MPU_MONITOR_RAMFUNC_WRITE_INDEX);

    mpu_set_address((u32_t)0x40000, \
        (u32_t)0x60000 - 1, MPU_DSP_WRITE, CONFIG_MPU_MONITOR_DSP_WRITE_MCU_MEMORY_INDEX);

    mpu_enable_region(CONFIG_MPU_MONITOR_DSP_WRITE_MCU_MEMORY_INDEX);
#endif


#ifdef CONFIG_MPU_MONITOR_ROMFUNC_WRITE
    /* ROM地址均是不可写的 */
    mpu_set_address(0, 0x30000 - 1, \
        (MPU_CPU_WRITE | MPU_DMA_WRITE), CONFIG_MPU_MONITOR_ROMFUNC_WRITE_INDEX);

    mpu_enable_region(CONFIG_MPU_MONITOR_ROMFUNC_WRITE_INDEX);
#endif

#ifdef CONFIG_MPU_MONITOR_STACK_OVERFLOW
    /*mpu3预留给任务栈检查使用，先初始化为无效值*/
    mpu_set_address(0, 2048 - 1, MPU_CPU_WRITE, CONFIG_MPU_MONITOR_STACK_OVERFLOW_INDEX);

    mpu_enable_region(CONFIG_MPU_MONITOR_STACK_OVERFLOW_INDEX);
#endif

    mpu_clear_all_pending();
}

extern uint32_t get_excpetion_user_register(uint32_t index);

#ifdef CONFIG_STACK_BACKTRACE
static int mpu_check_thread_stack(void)
{
    int len;

    uint32_t stack_cur;

    uint32_t stack_start;

    struct k_thread *thread = k_current_get();

    stack_start = thread->stack_info.start;

#ifdef CONFIG_MPU_EXCEPTION_DRIVEN
    stack_cur = get_excpetion_user_register(14);
#endif

#ifdef CONFIG_MPU_MPU_IRQ_DRIVEN
    stack_cur = get_user_sp();
#endif

    len =  stack_cur - stack_start;

	printk("current thread sp %x start %x\n", stack_cur, stack_start);

    return len;
}
#endif

static int mpu_analyse(void)
{
    int retval = 0;

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_ERROR)
    static const char * access_reason[] =
    {
        "dsp write",
        "dsp read",
        "cpu write",
        "dma write",
        "dsp iread"
    };
#endif

    unsigned int pending, mpux_pending, addr, offset, dst, size;
    int  i, dma;
    mpu_base_register_t *base_register;
    struct dma_regs *dma_base_regs;

    mpu_protect_disable();
    pending = sys_read32(MPUIP);

    base_register = (mpu_base_register_t *)MPUCTL0;

    dma_base_regs = (struct dma_regs *)DMA0CTL;

    for (i = 0; i < CONFIG_MPU_ACTS_MAX_INDEX; i++)
    {
        mpux_pending = (pending >> (i*8)) & 0x1f;
        if(!mpux_pending)
            continue;
        addr = base_register[i].MPUERRADDR;

        if(mpux_pending & MPU_DSP_WRITE)
        {
            retval = 1;
            SYS_LOG_ERR("<BUG> %s addr 0x%x\n", access_reason[0], mpu_dsp_to_mcu_addr(addr));
            continue;
        }
        else if(mpux_pending & MPU_CPU_WRITE)
        {
            if(i == 3)
            {
#ifdef CONFIG_STACK_BACKTRACE
                int len;
                len = mpu_check_thread_stack();

                if(len >= 0)
                {
                    SYS_LOG_ERR("Warning:task%d\' stack only %d bytes left!\n", os_thread_priority_get(os_current_get()), len);
                    dump_stack();
                    continue;
                }
                else
                {
                    retval = 1;
                    SYS_LOG_ERR("<BUG>:task%d\' stack overflow %d bytes!\n", os_thread_priority_get(os_current_get()), len * -1);
                }
#endif
            }
            else {
                retval = 1;
                SYS_LOG_ERR("<BUG> %s addr 0x%x\n", access_reason[2], addr);
            }
        }
        else /*if(mpux_pending & MPU_DMA_WRITE)*/
        {
            if(mpu_ignore_invalid_dma_fault() > 0)
                continue;
            for(dma = 0; dma < 5; dma++)
            {
                dst = dma_base_regs[dma].daddr0;
                size = dma_base_regs[dma].framelen;
                offset = addr & 0x3FFFF;
                if(offset >= dst && offset < dst + size)
                {
                    retval = 1;
                    SYS_LOG_ERR("<BUG> %s addr 0x%x, dma%d: dst=0x%x size=%d\n", access_reason[3], addr, dma, dst, size);
                    break;
                }
            }
        }

#ifndef CONFIG_MPU_EXCEPTION_DRIVEN
        panic("mpu error\n");
#endif

    }

    mpu_clear_all_pending();
    //mpu_protect_enable();

    return retval;
}

void mpu_protect_clear_pending(int mpu_no)
{
    sys_write32(0x1f << (8 * mpu_no), MPUIP);
}

void mpu_protect_enable(void)
{
    sys_write32(0xffffffff, MPUIE);
}

void mpu_protect_disable(void)
{
    sys_write32(0, MPUIE);
}

void mpu_enable_region(unsigned int index)
{
    mpu_base_register_t *base_register = (mpu_base_register_t *)MPUCTL0;

    base_register += index;

    base_register->MPUCTL |= (0x01);
}

void mpu_disable_region(unsigned int index)
{
    mpu_base_register_t *base_register = (mpu_base_register_t *)MPUCTL0;

    base_register += index;

    base_register->MPUCTL &= (~(0x01));
}

int mpu_region_is_enable(unsigned int index)
{
    mpu_base_register_t *base_register = (mpu_base_register_t *)MPUCTL0;

    base_register += index;

    return ((base_register->MPUCTL & 0x01) == 1);
}

#ifdef CONFIG_MPU_MONITOR_USER_DATA
int mpu_user_data_monitor(unsigned int start_addr, unsigned int end_addr)
{
    mpu_disable_region(CONFIG_MPU_MONITOR_USER_DATA_INDEX);

    /* TEXT地址均是不可写的 */
    mpu_set_address((unsigned int)start_addr, (unsigned int)end_addr - 1, \
        (MPU_CPU_WRITE | MPU_DMA_WRITE), CONFIG_MPU_MONITOR_USER_DATA_INDEX);

    mpu_enable_region(CONFIG_MPU_MONITOR_USER_DATA_INDEX);

    return 0;
}
#endif

int mpu_ignore_invalid_dma_fault(void)
{
    unsigned int pending, offset, addr, dst;

    //如果没有发生MPU pending直接返回
    pending = sys_read32(MPUIP);
    if(!pending)
        return 1;

    //如果没有发生DMA pending,直接返回
    if((pending & 0x08080808) == 0)
        return 0;

    offset = (uint32_t)MPUCTL0;
    while(pending)
    {
        if(pending & 0x08)
            break;

        pending >>= 8;
        offset += 16;
    }

    //读取ErrorAddr
    addr = sys_read32(offset + 12);

    //判断ErrorAddr是否在region内
    if(addr >= sys_read32(offset + 4) && addr <= sys_read32(offset + 8))
    {
        offset = (uint32_t)DMA0DADDR0;

        //查询最后一个DMA
        while(offset <= (uint32_t)(DMA0DADDR0 + DMA_ACTS_MAX_CHANNELS * DMA_CHAN_OFFS))
        {
            dst = sys_read32(offset);
            if(addr >= dst && addr < dst + sys_read32(offset + 8))
                return -1;

            offset += 0x18;
        }
    }
    sys_write32(0x08080808, MPUIP);
    return 1;
}

int mpu_handler(void)
{
    //ACT_LOG_ID_INF(ALF_STR_mpu_handler__MPU_ENTER_XN, 1, MEMORY_CONTROLLER_REGISTER_2.MPUIP);

	printk("** MPU exception **\n");

    //do_mpu_ignore_task_stack();
    if(mpu_ignore_invalid_dma_fault() <= 0)
        return mpu_analyse();
    return 0;
}

int mpu_init(struct device *arg)
{
    ARG_UNUSED(arg);

    mpu_protect_init();

    mpu_protect_enable();

    SYS_LOG_INF("mpu init\n");

#ifdef CONFIG_MPU_MPU_IRQ_DRIVEN
    //最高优先级，在任务系统之前处理
	IRQ_CONNECT(IRQ_ID_MPU, CONFIG_IRQ_PRIO_HIGHEST, mpu_handler, 0, 0);
	irq_enable(IRQ_ID_MPU);
#endif

#ifdef CONFIG_MPU_EXCEPTION_DRIVEN
    sys_write32(sys_read32(MEMORYCTL) | MEMORYCTL_BUSERROR_BIT, MEMORYCTL);
#endif

    return 0;
}

SYS_INIT(mpu_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

#else

void mpu_set_address(uint32_t start, uint32_t end, uint32_t flag, uint32_t index)
{
	ARG_UNUSED(start);
	ARG_UNUSED(end);
	ARG_UNUSED(flag);
	ARG_UNUSED(index);
	return;
}

void mpu_protect_enable(void)
{
	return;
}

void mpu_protect_disable(void)
{
	return;
}

void mpu_enable_region(unsigned int index)
{
	ARG_UNUSED(index);
	return;
}

void mpu_disable_region(unsigned int index)
{
	ARG_UNUSED(index);
	return;
}

int mpu_region_is_enable(unsigned int index)
{
	ARG_UNUSED(index);
	return 0;
}
#endif

