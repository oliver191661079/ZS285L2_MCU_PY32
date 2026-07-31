/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Driver for Independent Watchdog (IWDG) for STM32 MCUs
 *
 * Based on reference manual:
 *   STM32F101xx, STM32F102xx, STM32F103xx, STM32F105xx and STM32F107xx
 *   advanced ARM ® -based 32-bit MCUs
 *
 * Chapter 19: Independent watchdog (IWDG)
 *
 */

#include <errno.h>
#include <kernel.h>
#include <watchdog.h>
#include <soc.h>

#define WD_CTL_EN		(1 << 4)
#define WD_CTL_CLKSEL_MASK	(0x7 << 1)
#define WD_CTL_CLKSEL_SHIFT	(1)
#define WD_CTL_CLKSEL(n)	((n) << WD_CTL_CLKSEL_SHIFT)
#define WD_CTL_CLR		(1 << 0)
#define WD_CTL_IRQ_PD	(1 << 6)
#define WD_CTL_IRQ_EN	(1 << 5)

/* timeout: 176ms * 2^n */
#define WD_TIMEOUT_CLKSEL_TO_MS(n)	(176 << (n))
#define WD_TIMEOUT_MS_TO_CLKSEL(ms)	(find_msb_set(((ms) + 1) / 176))

static void (*g_irq_handler_wdg)(struct device *dev);

static void wdg_acts_enable(struct device *dev)
{
	ARG_UNUSED(dev);

	sys_write32(sys_read32(WD_CTL) | WD_CTL_EN, WD_CTL);
}

static void wdg_acts_disable(struct device *dev)
{
	ARG_UNUSED(dev);

	sys_write32(sys_read32(WD_CTL) & ~WD_CTL_EN, WD_CTL);
}

static void wdg_acts_clear_irq_pd(void)
{
	sys_write32(sys_read32(WD_CTL), WD_CTL);
}

static void wdg_irq_handler(void *arg)
{
	wdg_acts_clear_irq_pd();

	if (g_irq_handler_wdg)
		g_irq_handler_wdg(NULL);
}

static int wdg_acts_set_config(struct device *dev,
				struct wdt_config *cfg)
{
	int clksel;

	ARG_UNUSED(dev);

	clksel = WD_TIMEOUT_MS_TO_CLKSEL(cfg->timeout);

	if (cfg->mode == WDT_MODE_RESET) {
		g_irq_handler_wdg = NULL;
		irq_disable(IRQ_ID_WD);

		sys_write32((sys_read32(WD_CTL) & ~WD_CTL_CLKSEL_MASK)
			| WD_CTL_CLKSEL(clksel) , WD_CTL);

		sys_write32((sys_read32(WD_CTL) & ~WD_CTL_IRQ_EN), WD_CTL);
	}else{
		g_irq_handler_wdg = cfg->interrupt_fn;
		IRQ_CONNECT(IRQ_ID_WD, CONFIG_IRQ_PRIO_HIGHEST, wdg_irq_handler, NULL, 0);
		irq_enable(IRQ_ID_WD);

		sys_write32((sys_read32(WD_CTL) & ~WD_CTL_CLKSEL_MASK)
			| WD_CTL_CLKSEL(clksel) | WD_CTL_IRQ_EN, WD_CTL);

	}

	return 0;
}

static void wdg_acts_get_config(struct device *dev,
				struct wdt_config *cfg)
{
	int clksel;

	ARG_UNUSED(dev);

	clksel = (sys_read32(WD_CTL) & WD_CTL_CLKSEL_MASK) >> WD_CTL_CLKSEL_SHIFT;

	cfg->timeout = WD_TIMEOUT_CLKSEL_TO_MS(clksel);
	cfg->mode = WDT_MODE_RESET;
	cfg->interrupt_fn = NULL;
}

static void wdg_acts_reload(struct device *dev)
{
	ARG_UNUSED(dev);

	sys_write32(sys_read32(WD_CTL) | WD_CTL_CLR, WD_CTL);
}

static const struct wdt_driver_api wdg_acts_driver_api = {
	.enable = wdg_acts_enable,
	.disable = wdg_acts_disable,
	.get_config = wdg_acts_get_config,
	.set_config = wdg_acts_set_config,
	.reload = wdg_acts_reload,
};

static int wdg_acts_init(struct device *dev)
{
	sys_write32(0x0, WD_CTL);

#ifdef CONFIG_WDT_ACTS_START_AT_BOOT
	wdg_acts_enable(dev);
#endif

	return 0;
}

DEVICE_AND_API_INIT(wdg_acts, CONFIG_WDT_ACTS_DEVICE_NAME,
		    wdg_acts_init, NULL, NULL,
		    PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &wdg_acts_driver_api);
