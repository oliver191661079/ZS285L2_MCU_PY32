/********************************************************************************
 *                            USDK(ZS283A_clean)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-25-上午10:41:06             1.0             build this file
 ********************************************************************************/
/*!
 * \file     mpu_acts.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-25-上午10:41:06
 *******************************************************************************/

#ifndef MPU_ACTS_H_
#define MPU_ACTS_H_

/**
 * @brief Declare a minimum MPU guard alignment and size
 *
 * This specifies the minimum MPU guard alignment/size for the MPU.  This
 * will be used to denote the guard section of the stack, if it exists.
 *
 * One key note is that this guard results in extra bytes being added to
 * the stack.  APIs which give the stack ptr and stack size will take this
 * guard size into account.
 *
 * Stack is allocated, but initial stack pointer is at the end
 * (highest address).  Stack grows down to the actual allocation
 * address (lowest address).  Stack guard, if present, will comprise
 * the lowest MPU_GUARD_ALIGN_AND_SIZE bytes of the stack.
 *
 * As the stack grows down, it will reach the end of the stack when it
 * encounters either the stack guard region, or the stack allocation
 * address.
 *
 * ----------------------- <---- Stack allocation address + stack size +
 * |                     |            MPU_GUARD_ALIGN_AND_SIZE
 * |  Some thread data   | <---- Defined when thread is created
 * |        ...          |
 * |---------------------| <---- Actual initial stack ptr
 * |  Initial Stack Ptr  |       aligned to STACK_ALIGN_SIZE
 * |        ...          |
 * |        ...          |
 * |        ...          |
 * |        ...          |
 * |        ...          |
 * |        ...          |
 * |        ...          |
 * |        ...          |
 * |  Stack Ends         |
 * |---------------------- <---- Stack Buffer Ptr from API
 * |  MPU Guard,         |
 * |     if present      |
 * ----------------------- <---- Stack Allocation address
 *
 */
#define MPU_GUARD_ALIGN_AND_SIZE	32


#define     MPU_DSP_WRITE       0x01
#define     MPU_DSP_READ        0x02
#define     MPU_CPU_WRITE       0x04
#define     MPU_DMA_WRITE       0x08
#define     MPU_DSP_INST_READ   0x10

void mpu_set_address(uint32_t start, uint32_t end, uint32_t flag, uint32_t index);

void mpu_protect_enable(void);

void mpu_protect_disable(void);

void mpu_enable_region(unsigned int index);

void mpu_disable_region(unsigned int index);

int mpu_region_is_enable(unsigned int index);

int mpu_user_data_monitor(unsigned int start_addr, unsigned int end_addr);

#endif /* MPU_ACTS_H_ */
