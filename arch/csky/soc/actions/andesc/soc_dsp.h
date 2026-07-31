/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-12-ÏÂÎç12:53:37             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_dsp.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-12-ÏÂÎç12:53:37
 *******************************************************************************/

#ifndef SOC_DSP_H_
#define SOC_DSP_H_

#include <stdint.h>
#include <misc/util.h>
#include <soc_regs.h>
#include <soc_cmu.h>
#include <soc_irq.h>
#include <sys_io.h>
#include <soc_memctrl.h>

#define SOC_MAPPING_PSRAM_ADDR 0x02000000

#ifdef CONFIG_SOC_MAPPING_PSRAM_ADDR
/* trick that make all address are sram address */
#if (CONFIG_SOC_MAPPING_PSRAM_ADDR != SOC_MAPPING_PSRAM_ADDR)
#error "PSRAM MAPPING ADDR ERROR"
#endif
#endif

#define DSP_M2D_MAILBOX_REGISTER_BASE       (DSP_MAILBOX_REG_BASE + 0x00)
#define DSP_D2M_MAILBOX_REGISTER_BASE       (DSP_MAILBOX_REG_BASE + 0x10)
#define DSP_USER_REGION_REGISTER_BASE       (DSP_MAILBOX_REG_BASE + 0x20)
#define DSP_DEBUG_REGION_REGISTER_BASE      (DSP_MAILBOX_REG_BASE + 0x40)

#define DSP_FUNCTION_TRIGGER                (DSP_USER_REGION_REGISTER_BASE + 0x1C)
static inline uint32_t get_function_trigger(uint32_t function_id)
{
	return  sys_read32(DSP_FUNCTION_TRIGGER) & (1 << function_id);
}
static inline void set_function_trigger(uint32_t function_id)
{
	sys_write32(sys_read32(DSP_FUNCTION_TRIGGER) | (1 << function_id), DSP_FUNCTION_TRIGGER);
}

extern uint32_t addr_cpu2dsp(uint32_t addr, bool is_code);
extern uint32_t addr_dsp2cpu(uint32_t addr, bool is_code);

static unsigned int inline mcu_to_dsp_code_address(unsigned int mcu_addr)
{
#ifdef CONFIG_DSP
    return addr_cpu2dsp(mcu_addr, true);
#else
    return 0;
#endif
}

/* 0x00000 <= dsp_addr < 0x20000
 */
static unsigned int inline dsp_code_to_mcu_address(unsigned int dsp_addr)
{
#ifdef CONFIG_DSP
    return addr_dsp2cpu(dsp_addr, true);
#else
    return 0;
#endif
}


static unsigned int inline mcu_to_dsp_data_address(unsigned int mcu_addr)
{
#ifdef CONFIG_DSP
    return addr_cpu2dsp(mcu_addr, false);
#else
    return 0;
#endif
}


/* 0x20000 <= dsp_addr < 0x40000
 */
static unsigned int inline dsp_data_to_mcu_address(unsigned int dsp_addr)
{
#ifdef CONFIG_DSP
    return addr_dsp2cpu(dsp_addr, false);
#else
    return 0;
#endif
}

static unsigned int inline mcu_to_mpu_dsp_addr(unsigned int mcu_addr)
{
    return mcu_to_dsp_code_address(mcu_addr) * 2;
}

static unsigned int inline mpu_dsp_to_mcu_addr(unsigned int dsp_addr)
{
    return (dsp_code_to_mcu_address(dsp_addr / 2) & 0x3ffff);
}

static inline u32_t dsp_in_standby_mode(void)
{
	return ((sys_read32(CMU_DSP_WAIT) & 0xc) == 0xc);
}

static inline u32_t dsp_in_lightsleep_mode(void)
{
	return ((sys_read32(CMU_DSP_WAIT) & 0xc) == 0x4);
}

static inline void dsp_do_wait(void)
{
    sys_write32(sys_read32(CMU_DSP_WAIT) | BIT(CMU_DSP_WAIT_DSPWEN) | BIT(CMU_DSP_WAIT_DSPDEWS), CMU_DSP_WAIT);
}

static inline void dsp_undo_wait(void)
{
    sys_write32(sys_read32(CMU_DSP_WAIT) & ~(BIT(CMU_DSP_WAIT_DSPWEN) | BIT(CMU_DSP_WAIT_DSPDEWS)), CMU_DSP_WAIT);
}

static inline void dsp_soc_request_mem(void)
{
    // RAM1/RAM2/RAM3[todo RAM9/RAM10/RAM11]  forbit dsp access
    // RAM0/RAM4/RAM5/RAM6/RAM7         only dsp access    todo
    // RAM8                             share memery
#ifdef CONFIG_SOC_SERIES_ANDESC_P2
    sys_write32(sys_read32(ACCESSCTL) & ~(BIT(5)|BIT(7)), ACCESSCTL);
#else
    sys_write32(sys_read32(ACCESSCTL) & ~(BIT(3)|BIT(5)|BIT(7)), ACCESSCTL);
#endif
}

static inline void clear_dsp_pageaddr(void)
{
    sys_write32(0, DSP_PAGE_ADDR0);
    sys_write32(0, DSP_PAGE_ADDR0 + 4);
    sys_write32(0, DSP_PAGE_ADDR0 + 8);
    sys_write32(0, DSP_PAGE_ADDR0 + 12);
}

static inline void mem_controller_dsp_pageaddr_set(uint32_t index, uint32_t value)
{
    sys_write32(value, DSP_PAGE_ADDR0 + index * 4);
}

static void inline set_dsp_vector_addr(unsigned int dsp_addr)
{
    sys_write32(dsp_addr, DSP_VCT_ADDR);
}

static inline void clear_dsp_irq_pending(unsigned int irq)
{
    unsigned int dsp_irq = irq - IRQ_ID_OUT_USER0;
    do {
        sys_write32((1 << dsp_irq), INT_REQ_IN_PD);
    } while (sys_read32(INT_REQ_IN_PD) & (1 << dsp_irq));
}

static inline void clear_dsp_all_irq_pending(void)
{
    sys_write32(0x1f, INT_REQ_IN_PD);
}

static inline int do_request_dsp(int in_user)
{
    //clear info
    sys_write32(in_user, INT_REQ_OUT);

    // info
    sys_write32(2, INT_REQ_INT_OUT);

    // info
    sys_write32(0, INT_REQ_INT_OUT);

    return 0;
}

static inline void mcu_trigger_irq_to_dsp(void)
{
    // info
    sys_write32(2, INT_REQ_INT_OUT);
}

static inline void mcu_untrigger_irq_to_dsp(void)
{
    // info
    sys_write32(0, INT_REQ_INT_OUT);
}
#endif /* SOC_DSP_H_ */
