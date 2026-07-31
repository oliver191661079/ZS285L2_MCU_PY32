/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file trace thread interface
 */

#include <kernel_structs.h>
#include <mpu_acts.h>

#if defined(CONFIG_THREAD_STACK_INFO)
#if defined(CONFIG_MPU_MONITOR_STACK_OVERFLOW)
/*
 * @brief Configure MPU stack guard
 *
 * This function configures per thread stack guards reprogramming the MPU.
 * The functionality is meant to be used during context switch.
 *
 * @param thread thread info data structure.
 */
void configure_mpu_stack_guard(struct k_thread *thread)
{
    u32_t guard_size = MPU_GUARD_ALIGN_AND_SIZE;
    u32_t guard_start = thread->stack_info.start;

    mpu_disable_region(CONFIG_MPU_MONITOR_STACK_OVERFLOW_INDEX);
    
    mpu_set_address(guard_start, guard_start + guard_size, \
        MPU_CPU_WRITE, CONFIG_MPU_MONITOR_STACK_OVERFLOW_INDEX);
        
    mpu_enable_region(CONFIG_MPU_MONITOR_STACK_OVERFLOW_INDEX);
}
#endif
#endif

__ramfunc void trace_task_start( struct _kernel *kernel)
{
#if ((defined(CONFIG_THREAD_STACK_INFO) && defined(CONFIG_MPU_MONITOR_STACK_OVERFLOW)) || (defined CONFIG_MPU_MONITOR_BTC_MEMORY))
    struct k_thread *current = kernel->current;
#endif
#ifdef CONFIG_MPU_MONITOR_BTC_MEMORY
    if(current->base.prio == -13 || current->base.prio == -15){
        mpu_disable_region(CONFIG_MPU_MONITOR_BTC_MEMORY_INDEX);
    }else{
        mpu_enable_region(CONFIG_MPU_MONITOR_BTC_MEMORY_INDEX);
    }
#endif

#if defined(CONFIG_THREAD_STACK_INFO)
#if defined(CONFIG_MPU_MONITOR_STACK_OVERFLOW)
    configure_mpu_stack_guard(current);
#endif
#endif
}

__ramfunc void trace_task_end(void)
{

}


