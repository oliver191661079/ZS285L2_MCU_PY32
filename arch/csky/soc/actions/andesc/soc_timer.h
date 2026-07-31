/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-12-下午5:13:04             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_timer.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-12-下午5:13:04
 *******************************************************************************/

#ifndef SOC_TIMER_H_
#define SOC_TIMER_H_

#define     T0_CTL_EN                                                         5
#define     T0_CTL_RELO                                                       2
#define     T0_CTL_ZIEN                                                       1
#define     T0_CTL_ZIPD                                                       0

#define     MAX_TIMER_COUNT 0xFFFFFF

typedef struct {
    volatile uint32_t ctl;
    volatile uint32_t val;
    volatile uint32_t cnt;
} timer_register_t;

static inline void timer_start(timer_register_t *timer_register)
{
    uint32_t ctl = timer_register->ctl & (~(1 << T0_CTL_ZIPD));
    timer_register->ctl = ctl | (1 << T0_CTL_EN);
}

static inline void timer_stop(timer_register_t *timer_register)
{
    uint32_t ctl = timer_register->ctl & (~(1 << T0_CTL_ZIPD));
    ctl &= (~(1 << T0_CTL_EN));
    timer_register->ctl = ctl;
}

static inline void timer_reload_enable(timer_register_t *timer_register, uint32_t reload_enable)
{
    uint32_t ctl = timer_register->ctl & (~(1 << T0_CTL_ZIPD));
    if (reload_enable) {
        SYS_CLEAR_SET_REG(ctl, (1 << T0_CTL_RELO), (1 << T0_CTL_RELO));
    } else {
        SYS_CLEAR_MASK(ctl, (1 << T0_CTL_RELO));
    }

    timer_register->ctl = ctl;
}

static inline int timer_count_read(timer_register_t *timer_register)
{
    return timer_register->cnt;
}

static inline void timer_irq_set(timer_register_t *timer_register, uint32_t irq_enable)
{
    uint32_t ctl = timer_register->ctl & (~(1 << T0_CTL_ZIPD));
    if (irq_enable) {
        SYS_CLEAR_SET_REG(ctl, (1 << T0_CTL_ZIEN), (1 << T0_CTL_ZIEN));
    } else {
        SYS_CLEAR_MASK(ctl, (1 << T0_CTL_ZIEN));
    }

    timer_register->ctl = ctl;
}

static inline void timer_irq_pending_clear(timer_register_t *timer_register)
{
    timer_register->ctl |= (1 << T0_CTL_ZIPD);
}

extern void timer_reload(timer_register_t *timer_register, uint32_t value);

#endif /* SOC_TIMER_H_ */
