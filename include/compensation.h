/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2019-2-18-上午11:39:04             1.0             build this file
 ********************************************************************************/
/*!
 * \file     compensation.h
 * \brief
 * \author
 * \version  1.0
 * \date  2019-2-18-上午11:39:04
 *******************************************************************************/

#ifndef COMPENSATION_H_
#define COMPENSATION_H_

/*频偏写入efuse中*/
#define  RW_TRIM_CAP_EFUSE  (0)

/*频偏写入norflash中*/
#define  RW_TRIM_CAP_SNOR   (1)

typedef enum
{
    TRIM_CAP_WRITE_NO_ERROR,
    TRIM_CAP_WRITE_ERR_HW,
    TRIM_CAP_WRITE_ERR_NO_RESOURSE
} trim_cap_write_result_e;

typedef enum
{
    /*读取频偏正确*/
    TRIM_CAP_READ_NO_ERROR,
    /*读取频偏调整值，并非真正频偏值，需要加偏移*/
    TRIM_CAP_READ_ADJUST_VALUE,
    /*读取频偏出错*/
    TRIM_CAP_READ_ERR_HW,
    /*未写频偏值*/
    TRIM_CAP_READ_ERR_NO_WRITE,
    /*频偏值非法*/
    TRIM_CAP_ERAD_ERR_VALUE
} trim_cap_read_result_e;

extern int32_t freq_compensation_read(uint32_t *trim_cap, uint32_t mode);

extern int32_t freq_compensation_write(uint32_t *trim_cap, uint32_t mode);

extern void cap_temp_comp_start(void);

extern void cap_temp_comp_stop(void);

extern int32_t get_detect_temp(s8_t* temp);

    extern uint32_t cap_temp_comp_enabled(void);

#endif /* COMPENSATION_H_ */
