/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief PWM controller driver for Actions SoC
 */
#include <errno.h>
#include <kernel.h>
#include <board.h>
#include <pwm.h>
#include <gpio.h>
#include <device.h>
#include <soc.h>
#include <soc.h>
#include <board.h>
#include "soc_cmu.h"
#include <logging/sys_log.h>
#include <string.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_PWM_ACTS

enum{
    INITI_START,
    INITI_WAIT,
    INITI_FINSIH,
};
#define CMU_PWMCLK_BASE         CMU_PWM0CLK
#define CMU_PWMCLK_CLKSEL_MASK		(0x1 << 9)
#define CMU_PWMCLK_CLKSEL_32K		(0x0 << 9)
#define CMU_PWMCLK_CLKSEL_HOSC		(0x1 << 9)
#define CMU_PWMCLK_CLKDIV_SHIFT		0
#define CMU_PWMCLK_CLKDIV(x)		((x) << CMU_PWMCLK_CLKDIV_SHIFT)
#define CMU_PWMCLK_CLKDIV_MASK		CMU_PWMCLK_CLKDIV(0x1ff)

#define PWM_CTRL_DUTY_SHIFT		0
#define PWM_CTRL_DUTY(x)		((x) << PWM_CTRL_DUTY_SHIFT)
#define PWM_CTRL_DUTY_MASK		PWM_CTRL_DUTY(0xff)
#define PWM_CTRL_L_SHIFT		8
#define PWM_CTRL_L(x)			((x) << PWM_CTRL_L_SHIFT)
#define PWM_CTRL_L_MASK			PWM_CTRL_L(0xff)
#define PWM_CTRL_H_SHIFT		16
#define PWM_CTRL_H(x)			((x) << PWM_CTRL_H_SHIFT)
#define PWM_CTRL_H_MASK			PWM_CTRL_H(0xff)
#define PWM_CTRL_Q_SHIFT		24
#define PWM_CTRL_Q(x)			((x) << PWM_CTRL_Q_SHIFT)
#define PWM_CTRL_Q_MASK			PWM_CTRL_Q(0xff)
#define PWM_CTRL_MODE_SEL_MASK		(0x1 << 26)
#define PWM_CTRL_MODE_SEL_NORMAL	(0x0 << 26)
#define PWM_CTRL_MODE_SEL_BREATH	(0x1 << 26)
#define PWM_CTRL_POL_SEL_MASK		(0x1 << 27)
#define PWM_CTRL_POL_SEL_ACTIVE_LOW	(0x0 << 27)
#define PWM_CTRL_POL_SEL_ACTIVE_HIGH	(0x1 << 27)
#define PWM_CTRL_CHAN_EN		(0x1 << 28)

#if (CONFIG_PWM_CLK_SRC == 0)
#define PWM_CLK_CYCLES_PER_SEC		(32000UL)
#elif (CONFIG_PWM_CLK_SRC == 1)
#define PWM_CLK_CYCLES_PER_SEC		(24000000UL)
#else
#define PWM_CLK_CYCLES_PER_SEC		(64000000UL)
#endif

#if (CONFIG_PWM_CLK_DIV > 255)
#define PWM_PIN_CYCLES_PER_SEC		(PWM_CLK_CYCLES_PER_SEC / (CONFIG_PWM_CLK_DIV + 1) / 2)
#else
#define PWM_PIN_CYCLES_PER_SEC		(PWM_CLK_CYCLES_PER_SEC / (CONFIG_PWM_CLK_DIV + 1))
#endif

#define PWM_PIN_CLK_PERIOD_USEC		(1000000UL / PWM_PIN_CYCLES_PER_SEC)

#define PWM_BREATH_MODE_DEFAULT_C	(32)

/* dma global registers */
struct acts_pwm_chan {
	volatile u32_t pwm_ctrl;
};

struct pwm_acts_config {
	u32_t	base;
	u32_t	pwmclk_reg;
	u32_t	num_chans;
	u8_t	clock_id;
	u8_t	reset_id;
};


/* convenience defines */
#define DEV_CFG(dev)							\
	((const struct pwm_acts_config * const)(dev)->config->config_info)
#define PWM_CHAN(base, chan_id)							\
	(((struct acts_pwm_chan *)(base)) + chan_id)


const struct acts_pin_config board_led_pin_config[] = {
#ifdef BOARD_PWM_MAP
    BOARD_PWM_MAP
#endif
};



const led_map pwmledmaps[] = {
#ifdef BOARD_LED_MAP
    BOARD_LED_MAP
#endif
};

static u8_t  pwm_init_step;
static u32_t pwm_time_init_start_time;
#define PWM_DELAY_TIME 400 //MS

/*
 * Set the period and pulse width for a PWM pin.
 *
 * Parameters
 * dev: Pointer to PWM device structure
 * pwm: PWM channel to set
 * period_cycles: Period (in timer count)
 * pulse_cycles: Pulse width (in timer count).
 *
 * return 0, or negative errno code
 */
static int pwm_acts_pin_set(struct device *dev, u32_t chan,
			     u32_t period_cycles, u32_t pulse_cycles, pwm_flags_t flags)
{
	const struct pwm_acts_config *cfg = DEV_CFG(dev);
	struct acts_pwm_chan *pwm_chan;

	struct device *dev_gpio;
	u8_t pin = 255 , i;

	SYS_LOG_DBG("%d, %d/%d, %d, %d", chan, pulse_cycles, period_cycles, flags, pwm_init_step);

	if (chan >= cfg->num_chans) {
		return -EINVAL;
	}

	if (period_cycles > 32 * 255) {
		return -EINVAL;
	}

	dev_gpio = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!dev_gpio) {
		return -1;
	}

	if(pwm_init_step ==INITI_WAIT){
		if ((pwm_time_init_start_time + PWM_DELAY_TIME) > k_uptime_get_32()) {
			for (i = 0; i < (ARRAY_SIZE(pwmledmaps)); i++) {
				if (pwmledmaps[i].led_pwm == chan) {
					pin = pwmledmaps[i].led_pin;
					break;
				}
			}
			if (i == (ARRAY_SIZE(pwmledmaps))) {
				return -EIO;
			}
		} else {
			acts_pinmux_setup_pins(board_led_pin_config, ARRAY_SIZE(board_led_pin_config));
			pwm_init_step = INITI_FINSIH;
		}
	}
	pwm_chan = PWM_CHAN(cfg->base, chan);

	/* disable pwm */
	pwm_chan->pwm_ctrl = 0;

	/* setup pwm parameters */
	if (pulse_cycles == period_cycles){
	        if(pwm_init_step == INITI_WAIT){
                    gpio_pin_write(dev_gpio, pin, (START_VOLTAGE_HIGH == flags)?1:0);
	        }
		pwm_chan->pwm_ctrl = PWM_CTRL_MODE_SEL_NORMAL | PWM_CTRL_DUTY(255);
		if (START_VOLTAGE_HIGH == flags) {
			pwm_chan->pwm_ctrl |= PWM_CTRL_POL_SEL_ACTIVE_HIGH;
		}
	} else if (pulse_cycles == 0){
	        if(pwm_init_step == INITI_WAIT){
                    gpio_pin_write(dev_gpio, pin, (START_VOLTAGE_HIGH == flags)?0:1);
	        }
		pwm_chan->pwm_ctrl = PWM_CTRL_MODE_SEL_NORMAL | PWM_CTRL_DUTY(255);
		if (START_VOLTAGE_HIGH == flags) {
			pwm_chan->pwm_ctrl |= PWM_CTRL_POL_SEL_ACTIVE_HIGH;
		}
	} else {
	        if(pwm_init_step == INITI_WAIT){
                    gpio_pin_write(dev_gpio, pin, (START_VOLTAGE_HIGH == flags)?0:1);
	        }
		if (START_VOLTAGE_HIGH == flags) {
			pwm_chan->pwm_ctrl = PWM_CTRL_POL_SEL_ACTIVE_HIGH|
					PWM_CTRL_MODE_SEL_NORMAL |
					PWM_CTRL_DUTY((pulse_cycles) * 255 / period_cycles);
		}else{
			pwm_chan->pwm_ctrl = PWM_CTRL_POL_SEL_ACTIVE_HIGH|
					PWM_CTRL_MODE_SEL_NORMAL |
					PWM_CTRL_DUTY((period_cycles - pulse_cycles) * 255 / period_cycles);
		}
	}
	/* enable pwm */
	pwm_chan->pwm_ctrl |= PWM_CTRL_CHAN_EN;

	return 0;
}

/*
 * Get the clock rate (cycles per second) for a PWM pin.
 *
 * Parameters
 * dev: Pointer to PWM device structure
 * pwm: PWM port number
 * cycles: Pointer to the memory to store clock rate (cycles per second)
 *
 * return 0, or negative errno code
 */
static int pwm_acts_get_cycles_per_sec(struct device *dev, u32_t chan,
					u64_t *cycles)
{
	//const struct pwm_acts_config *cfg = DEV_CFG(dev);
	u32_t pwmclk, pclk, clkdiv;

    pwmclk = sys_read32(CMU_PWMCLK_BASE + 4 * chan);
    if ((pwmclk & CMU_PWMCLK_CLKSEL_32K) == CMU_PWMCLK_CLKSEL_32K)
		pclk = 32768;
	else
		pclk = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC;

    clkdiv = pwmclk & CMU_PWMCLK_CLKDIV_MASK;
    if (clkdiv < 256){
        clkdiv ++;
    }else if( clkdiv < 261){
        clkdiv =clkdiv - 256;
        clkdiv = 512 << clkdiv;
    }else{
        return -1;
    }

    //clkdiv <<= 8;

    // ACT_LOG_ID_INF(ALF_STR_pwm_acts_get_cycles_per_sec__DRV_CLK_D_DIV_D_PWMC, 3, pclk, clkdiv, pwmclk);

    *cycles = pclk / clkdiv;

    return 0;
}

static inline int pwm_acts_duty_set(struct device *dev, u32_t chan, u32_t duty)
{
	const struct pwm_acts_config *cfg = DEV_CFG(dev);
	struct acts_pwm_chan *pwm_chan;

	pwm_chan = PWM_CHAN(cfg->base, chan);
	if(duty == 0)
	{
		/* disable pwm */
		pwm_chan->pwm_ctrl &= ~PWM_CTRL_CHAN_EN;
	}
	else
	{
		unsigned int reg = PWM_CTRL_POL_SEL_ACTIVE_LOW |
			PWM_CTRL_MODE_SEL_NORMAL | PWM_CTRL_DUTY(255-duty) | PWM_CTRL_CHAN_EN;

		/* enable pwm */
		pwm_chan->pwm_ctrl = reg;
	}
	return 0;
}

static int pwm_acts_set_breath_mode(struct device *dev, u32_t chan, pwm_breath_ctrl_t *ctrl)
{
	const struct pwm_acts_config *cfg = DEV_CFG(dev);
	struct acts_pwm_chan *pwm_chan;
	u32_t period = PWM_PIN_CLK_PERIOD_USEC, qd, qu, high, low;
	pwm_breath_ctrl_t breath_ctrl = {0};

	if (!ctrl) {
		breath_ctrl.rise_time_ms = PWM_BREATH_RISE_TIME_DEFAULT;
		breath_ctrl.down_time_ms = PWM_BREATH_DOWN_TIME_DEFAULT;
		breath_ctrl.high_time_ms = PWM_BREATH_HIGH_TIME_DEFAULT;
		breath_ctrl.low_time_ms = PWM_BREATH_LOW_TIME_DEFAULT;
	} else {
		memcpy(&breath_ctrl, ctrl, sizeof(pwm_breath_ctrl_t));
	}

	SYS_LOG_DBG("PWM@%d rise %dms, down %dms, high %dms, low %dms",
		chan, breath_ctrl.rise_time_ms, breath_ctrl.down_time_ms,
		breath_ctrl.high_time_ms, breath_ctrl.low_time_ms);

	if (chan >= cfg->num_chans) {
		SYS_LOG_ERR("invalid chan %d", chan);
		return -EINVAL;
	}
	pwm_chan = PWM_CHAN(cfg->base, chan);

	/* disable pwm */
	pwm_chan->pwm_ctrl = 0;

	/* setup pwm parameters */

	/* rise time T1 = QU x C x C x t; C=32, t=PWM_PIN_CLK_PERIOD_USEC */
	qu = breath_ctrl.rise_time_ms * 1000 / PWM_BREATH_MODE_DEFAULT_C / PWM_BREATH_MODE_DEFAULT_C;
	qu = (qu + period - 1) / period;

	/* down time T2 = QD x C x C x t; C=32, t=PWM_PIN_CLK_PERIOD_USEC */
	qd = breath_ctrl.down_time_ms * 1000 / PWM_BREATH_MODE_DEFAULT_C / PWM_BREATH_MODE_DEFAULT_C;
	qd = (qd + period - 1) / period;

	/* high level time T3 = H x C x t; C=32, t = PWM_PIN_CLK_PERIOD_USEC */
	high = breath_ctrl.high_time_ms * 1000 / PWM_BREATH_MODE_DEFAULT_C;
	high = (high + period - 1) / period;

	/* low level time T3 = L x C x t; C=32, t = PWM_PIN_CLK_PERIOD_USEC */
	low = breath_ctrl.low_time_ms * 1000 / PWM_BREATH_MODE_DEFAULT_C;
	low = (low + period - 1) / period;

	SYS_LOG_DBG("QU:%d QD:%d high:%d low:%d", qu, qd, high, low);
	sys_write32(CMU_PWMCLK_CLKSEL_32K | CMU_PWMCLK_CLKDIV(CONFIG_PWM_CLK_DIV),
		    cfg->pwmclk_reg + 4 * chan);
	pwm_chan->pwm_ctrl = PWM_CTRL_MODE_SEL_BREATH |
			     PWM_CTRL_Q(qu) |
			     PWM_CTRL_H(high) |
			     PWM_CTRL_L(low);

	/* enable pwm */
	pwm_chan->pwm_ctrl |= PWM_CTRL_CHAN_EN;

	return 0;
}

static int pwm_acts_clock_set(struct device *dev, u32_t chan, u32_t clock)
{
	const struct pwm_acts_config *cfg = DEV_CFG(dev);

	if (chan >= cfg->num_chans) {
		return -EINVAL;
	}

	SYS_LOG_DBG("Clock:%d\n", clock);

	sys_write32(CMU_PWMCLK_CLKSEL_32K | CMU_PWMCLK_CLKDIV(32*1024/clock - 1), cfg->pwmclk_reg + 4 * chan);

	return 0;
}

static void pwm_pin_init_start(void)
{
    struct device *dev_gpio,*dev_pwm;
    int i;
    #define MAX_DUTY (32*255)
    pwm_init_step = INITI_START ;
    pwm_time_init_start_time = k_uptime_get_32();

    dev_gpio = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
    dev_pwm = device_get_binding(CONFIG_PWM_ACTS_DEV_NAME);

	if((!dev_gpio) || (!dev_pwm)){
		return;
	}

    for (i = 0; i <  (ARRAY_SIZE(board_led_pin_config)); i++) {
        acts_pinmux_set(board_led_pin_config[i].pin_num, GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP);
        gpio_pin_configure(dev_gpio, board_led_pin_config[i].pin_num, GPIO_DIR_OUT  );
        gpio_pin_write(dev_gpio, board_led_pin_config[i].pin_num, pwmledmaps[i].active_level?0:1);
        pwm_pin_set_cycles(dev_pwm, pwmledmaps[i].led_pwm, MAX_DUTY, MAX_DUTY, pwmledmaps[i].active_level?START_VOLTAGE_LOW:START_VOLTAGE_HIGH);
    }
    pwm_init_step = INITI_WAIT;
}
const struct pwm_driver_api pwm_acts_drv_api_funcs = {
	.pin_set = pwm_acts_pin_set,
	.duty_set = pwm_acts_duty_set,
	.get_cycles_per_sec = pwm_acts_get_cycles_per_sec,
	.set_breath = pwm_acts_set_breath_mode,
	.clock_set = pwm_acts_clock_set,
};

int pwm_acts_init(struct device *dev)
{
	const struct pwm_acts_config *cfg = DEV_CFG(dev);
	int i;

	/* enable pwm controller clock */
	acts_clock_peripheral_enable(cfg->clock_id);

	/* reset pwm controller */
	acts_reset_peripheral(cfg->reset_id);

	/* init PWM clock */
	for (i = 0; i < cfg->num_chans; i++) {
		/* clock source: 32K, div= / 32, pwm clock is 1KHz */
		sys_write32(CMU_PWMCLK_CLKSEL_32K | CMU_PWMCLK_CLKDIV(CONFIG_PWM_CLK_DIV), \
			cfg->pwmclk_reg + 4 * i);
	}

        pwm_pin_init_start();
        return 0;
}

static const struct pwm_acts_config pwm_acts_dev_cfg = {
	.base = PWM_REG_BASE,
	.pwmclk_reg = CMU_PWM0CLK,
	.num_chans = 9,
	.clock_id = CLOCK_ID_PWM,
	.reset_id = RESET_ID_PWM,
};

DEVICE_AND_API_INIT(pwm_acts, CONFIG_PWM_ACTS_DEV_NAME,
		    pwm_acts_init,
		    NULL, &pwm_acts_dev_cfg,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &pwm_acts_drv_api_funcs);
