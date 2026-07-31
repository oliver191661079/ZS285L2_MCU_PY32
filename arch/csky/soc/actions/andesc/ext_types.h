/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-13-下午1:25:30             1.0             build this file
 ********************************************************************************/
/*!
 * \file     ext_types.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-13-下午1:25:30
 *******************************************************************************/
#ifndef EXT_TYPES_H_
#define EXT_TYPES_H_

#ifndef  NULL
#define  NULL ((void *)0)
#endif

#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE   0
#endif

#ifndef  bool
#define  bool int
#endif

#ifndef SYS_BIT
#define SYS_BIT(n)  (1UL << (n))
#endif

#ifndef SYS_2BIT
#define SYS_2BIT(n)  (3UL << (n))
#endif

#ifndef SYS_3BIT
#define SYS_3BIT(n)  (7UL << (n))
#endif

#define SYS_SET_BIT(REG, BIT)     ((REG) |= (SYS_BIT(BIT)))
#define SYS_CLEAR_BIT(REG, BIT)   ((REG) &= ~(SYS_BIT(BIT)))
#define SYS_READ_BIT(REG, BIT)    ((REG) & (SYS_BIT(BIT)))
#define SYS_TEST_BIT(REG, BIT)    (((REG) & (SYS_BIT(BIT))) != 0)

#define SYS_CLEAR_MASK(REG, CLEARMASK)        ((REG) &= ~(CLEARMASK))
#define SYS_SET_MASK(REG, SETMASK)          ((REG) |= (SETMASK))
#define SYS_WRITE_REG(REG, VAL)              ((REG) = (VAL))
#define SYS_READ_REG(REG)                     (REG)
#define SYS_CLEAR_SET_REG(REG, CLEARMASK, SETMASK)  SYS_WRITE_REG((REG), (((SYS_READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))

//pending类寄存器是写1清0的，不需要操作其它bit位
#define SYS_CLEAR_BIT_PD(REG, BIT)    ((REG) = (SYS_BIT(BIT)))
#define SYS_CLEAR_REG_PD(REG)         ((REG) = (REG))

#ifndef ABS
#define ABS(a) ((a)>0?(a):-(a))
#endif

#endif /* EXT_TYPES_H_ */
