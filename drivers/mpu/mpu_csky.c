/********************************************************************************
 *                            USDK(ZS283A_clean)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-31-下午7:36:01             1.0             build this file
 ********************************************************************************/
/*!
 * \file     csky_mpu.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-31-下午7:36:01
 *******************************************************************************/
#include <init.h>
#include <kernel.h>
#include <arch/csky/csi.h>
#include <linker/linker-defs.h>

static void csky_mpu_init(void)
{
    mpu_region_attr_t  attr_test;
    attr_test.nx = 0;
    attr_test.ap = AP_BOTH_RW;
    attr_test.s = 0;

    csi_mpu_config_region(0, 0, REGION_SIZE_4GB, attr_test, 1);

    csi_mpu_enable();

#ifdef CONFIG_MPU_CSKY_MONITOR_ROMFUNC_WRITE

    attr_test.nx = 0;
    attr_test.ap = AP_SUPER_RW_USER_RDONLY;
    attr_test.s = 0;

    csi_mpu_config_region(1, 0, REGION_SIZE_128KB, attr_test, 1);

    csi_mpu_enable_region(1);

    csi_mpu_config_region(2, 128 * 1024, REGION_SIZE_64KB, attr_test, 1);

    csi_mpu_enable_region(2);
#endif

#ifdef CONFIG_MPU_CSKY_MONITOR_CACHEREGION_WRITE
    csi_mpu_config_region(3, (unsigned int)&_image_rom_start, REGION_SIZE_2MB, attr_test, 1);

    csi_mpu_enable_region(3);
#endif
}

SYS_INIT(csky_mpu_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

