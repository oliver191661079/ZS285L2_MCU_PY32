/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2019-3-30-上午11:40:50             1.0             build this file
 ********************************************************************************/
/*!
 * \file     system.h
 * \brief
 * \author
 * \version  1.0
 * \date  2019-3-30-上午11:40:50
 *******************************************************************************/

#ifndef SYSTEM_H_
#define SYSTEM_H_

/*!
 * \brief vram读接口
 */
extern int sys_vm_read(void* buf, const char *vm_name, int size);

/*!
 * \brief vram写接口
 */
extern int sys_vm_write(void* buf, const char *vm_name, int size);


/*!
 * \brief vram读factory_nvram_region接口
 */

extern int sys_vm_read_factory_nvram_region(void* buf, const char *vm_name, int size);

/*!
 * \brief vram写入factory_nvram_region 接口
 */
extern int sys_vm_write_factory_nvram_region(void* buf, const char *vm_name, int size);
/*!
 * \brief 计算时间间隔差值
 */
extern uint32_t get_diff_time(uint32_t end_time, uint32_t begin_time);


/*!
 * \brief 线程微秒级别的挂起操作
 */
extern void thread_usleep(uint32_t usec);

#endif /* SYSTEM_H_ */
