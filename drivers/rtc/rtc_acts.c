/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief RTC driver for Actions SoC
 */

#include <errno.h>
#include <device.h>
#include <init.h>
#include <kernel.h>
#include <string.h>
#include <rtc.h>
#include <soc.h>
#include <soc_pm.h>

#define SYS_LOG_DOMAIN "RTC"
#include <logging/sys_log.h>

#define RTC_DEFAULT_INIT_TIME	      (1483200000)	/* 2016.12.31  16:00:00 */

/* RTC Control Register */
#define	RTC_CTL_ALIP                  (1 << 0)
#define	RTC_CTL_ALIE                  (1 << 1)
#define	RTC_CTL_CALEN                 (1 << 4)
#define	RTC_CTL_LEAP                  (1 << 7)
#define	RTC_CTL_TESTEN                (1 << 16)

#define	RTC_CTL_CLKSEL_SHIFT          (2)
#define	RTC_CTL_CLKSEL(x)             ((x) << RTC_CTL_CLKSEL_SHIFT)
#define	RTC_CTL_CLKSEL_MASK           RTC_CTL_CLKSEL(3)
#define RTC_CLKSEL_HCL_DIV            RTC_CTL_CLKSEL(0) /* HOSC calibrate LOSC and divison*/
#define RTC_CLKSEL_BI_OSC             RTC_CTL_CLKSEL(1) /* Build-in OSC */
#define RTC_CLKSEL_HIGH_DIV           RTC_CTL_CLKSEL(2) /* High Clock and divison */

#define RTC_YMD_Y_SHIFT               (16)
#define RTC_YMD_Y_MASK                (0x7f << RTC_YMD_Y_SHIFT)
#define RTC_YMD_M_SHIFT               (8)
#define RTC_YMD_M_MASK                (0xf << RTC_YMD_M_SHIFT)
#define RTC_YMD_D_SHIFT               (0)
#define RTC_YMD_D_MASK                (0x1f << RTC_YMD_D_SHIFT)

#define RTC_YMD_Y(ymd)                (((ymd) & RTC_YMD_Y_MASK) >> RTC_YMD_Y_SHIFT)
#define RTC_YMD_M(ymd)                (((ymd) & RTC_YMD_M_MASK) >> RTC_YMD_M_SHIFT)
#define RTC_YMD_D(ymd)                (((ymd) & RTC_YMD_D_MASK) >> RTC_YMD_D_SHIFT)
#define RTC_YMD_VAL(y, m, d)          (((y) << RTC_YMD_Y_SHIFT) | ((m) << RTC_YMD_M_SHIFT) | ((d) << RTC_YMD_D_SHIFT))

#define RTC_HMS_H_SHIFT               (16)
#define RTC_HMS_H_MASK                (0x1f << RTC_HMS_H_SHIFT)
#define RTC_HMS_M_SHIFT	              (8)
#define RTC_HMS_M_MASK                (0x3f << RTC_HMS_M_SHIFT)
#define RTC_HMS_S_SHIFT               (0)
#define RTC_HMS_S_MASK                (0x3f << RTC_HMS_S_SHIFT)

#define RTC_HMS_H(ymd)                (((ymd) & RTC_HMS_H_MASK) >> RTC_HMS_H_SHIFT)
#define RTC_HMS_M(ymd)                (((ymd) & RTC_HMS_M_MASK) >> RTC_HMS_M_SHIFT)
#define RTC_HMS_S(ymd)                (((ymd) & RTC_HMS_S_MASK) >> RTC_HMS_S_SHIFT)
#define RTC_HMS_VAL(h, m, s)          (((h) << RTC_HMS_H_SHIFT) | ((m) << RTC_HMS_M_SHIFT) | ((s) << RTC_HMS_S_SHIFT))

#define HCL_4HZ_LOSC_EN               (1 << 0)

struct acts_rtc_controller {
	volatile u32_t ctrl;
	volatile u32_t regupdata;
	volatile u32_t hms_alarm;
	volatile u32_t hms;
	volatile u32_t ymd;
	volatile u32_t access;
};

struct acts_rtc_data {
	struct acts_rtc_controller *base;
	void (*alarm_cb_fn)(const void *cb_data);
	const void *cb_data;
	bool alarm_en;
	u8_t alarm_wakeup : 1; /* System wakeup from RTC alarm indicator */
};

struct acts_rtc_config {
	struct acts_rtc_controller *base;
	void (*irq_config)(void);
};

static void rtc_acts_dump_regs(struct acts_rtc_controller *base)
{
	SYS_LOG_INF("** RTC Controller register **");
	SYS_LOG_INF("      BASE: 0x%x", (u32_t)base);
	SYS_LOG_INF("       CTL: 0x%x", base->ctrl);
	SYS_LOG_INF(" REGUPDATE: 0x%x", base->regupdata);
	SYS_LOG_INF("   DHMSALM: 0x%x", base->hms_alarm);
	SYS_LOG_INF("      DHMS: 0x%x", base->hms);
	SYS_LOG_INF("       YMD: 0x%x", base->ymd);
	SYS_LOG_INF("    ACCESS: 0x%x", base->access);
}

/* Select the LOSC as the clock source which is inside of the HCL controller. */
static inline void rtc_acts_init_clksource(struct acts_rtc_data *rtcd)
{
	struct acts_rtc_controller *base = rtcd->base;
	base->ctrl &= ~RTC_CTL_CLKSEL_MASK;

	base->ctrl |= RTC_CLKSEL_HCL_DIV;
	sys_write32(sys_read32(HCL_4HZ) | HCL_4HZ_LOSC_EN, HCL_4HZ);

	sys_pm_update_rtc_regs();
}

static int rtc_acts_get_datetime(struct acts_rtc_data *rtcd, struct rtc_time *tm)
{
	struct acts_rtc_controller *base = rtcd->base;
	u32_t ymd, hms;

	ymd = base->ymd;
	hms = base->hms;

	tm->tm_sec = RTC_HMS_S(hms);		/* rtc second 0-59 */
	tm->tm_min = RTC_HMS_M(hms);		/* rtc minute 0-59 */
	tm->tm_hour = RTC_HMS_H(hms);		/* rtc hour 0-23 */
	tm->tm_mday = RTC_YMD_D(ymd);		/* rtc day 1-31 */
	tm->tm_wday = 0;
	tm->tm_mon = RTC_YMD_M(ymd) - 1;	/* rtc mon 1-12 */
	tm->tm_year = 100 + RTC_YMD_Y(ymd);	/* rtc year 0-99 */

	SYS_LOG_DBG("read date time: %04d-%02d-%02d  %02d:%02d:%02d",
		1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	/* the clock can give out invalid datetime, but we cannot return
	 * -EINVAL otherwise hwclock will refuse to set the time on bootup.
	 */
	if (rtc_valid_tm(tm) < 0) {
		SYS_LOG_ERR("rtc: retrieved date/time is not valid.\n");
		rtc_acts_dump_regs(base);
	}
	return 0;
}

static int rtc_acts_set_datetime(struct acts_rtc_data *rtcd, struct rtc_time *tm)
{
	struct acts_rtc_controller *base = rtcd->base;

	SYS_LOG_INF("set datetime: %04d-%02d-%02d  %02d:%02d:%02d",
               1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
               tm->tm_hour, tm->tm_min, tm->tm_sec);

	/* disable calendar when update the hms register
	 *  From the specification discription, the CAL_EN bit must be disable  when
	 *  the RTC_DHMS/RTC_YMD registers being written. And RTC_DHMS/RTC_YMD
	 *  registers must be written before CAL_EN is enabled.
	 */
	base->ctrl &= ~RTC_CTL_CALEN;
	sys_pm_update_rtc_regs();
	base->ymd = RTC_YMD_VAL(tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday);
	base->hms = RTC_HMS_VAL(tm->tm_hour, tm->tm_min, tm->tm_sec);
	sys_pm_update_rtc_regs();
	base->ctrl |= RTC_CTL_CALEN;
	sys_pm_update_rtc_regs();

	return 0;
}

static void rtc_acts_set_alarm_interrupt(struct acts_rtc_controller *base, bool enable)
{
	if (enable)
		base->ctrl |= RTC_CTL_ALIE;
	else
		base->ctrl &= ~RTC_CTL_ALIE;

	sys_pm_update_rtc_regs();
}

/* get & clear alarm irq pending */
static uint32_t rtc_acts_get_pending_int(struct device *dev)
{
	struct acts_rtc_data *rtcd = dev->driver_data;
	struct acts_rtc_controller *base = rtcd->base;
	int pending;

	pending = base->ctrl & RTC_CTL_ALIP;
	if (pending) {
		/* clear pending */
		base->ctrl |= RTC_CTL_ALIP;
		SYS_LOG_INF("Clear old RTC alarm pending");
		sys_pm_update_rtc_regs();
	}

	return pending;
}

static void rtc_acts_enable(struct device *dev)
{
	struct acts_rtc_data *rtcd = dev->driver_data;
	struct acts_rtc_controller *base = rtcd->base;

	base->ctrl |= RTC_CTL_CALEN;
	sys_pm_update_rtc_regs();
}

static void rtc_acts_disable(struct device *dev)
{
	struct acts_rtc_data *rtcd = dev->driver_data;
	struct acts_rtc_controller *base = rtcd->base;

	base->ctrl &= ~RTC_CTL_CALEN;
	sys_pm_update_rtc_regs();
}

static int rtc_acts_get_time(struct device *dev, struct rtc_time *tm)
{
	struct acts_rtc_data *rtcd = dev->driver_data;

	if (!tm)
		return -EINVAL;

	if (rtc_acts_get_datetime(rtcd, tm)) {
		SYS_LOG_ERR("failed to get datetime");
		return -EACCES;
	}

	return 0;
}

static int rtc_acts_set_time(struct device *dev, const struct rtc_time *tm)
{
	struct acts_rtc_data *rtcd = dev->driver_data;

	if (!tm)
		return -EINVAL;

	if (rtc_valid_tm((struct rtc_time *)tm)) {
		SYS_LOG_ERR("Bad time structure");
		print_rtc_time((struct rtc_time *)tm);
		return -ENOEXEC;
	}

	return rtc_acts_set_datetime(rtcd, (struct rtc_time *)tm);
}

static int rtc_acts_set_alarm_time(struct acts_rtc_data *rtcd, struct rtc_time *tm)
{
	struct acts_rtc_controller *base = rtcd->base;

	SYS_LOG_INF("set alarm: %04d-%02d-%02d  %02d:%02d:%02d",
		1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	/* disable alarm interrupt */
	rtc_acts_set_alarm_interrupt(base, false);

	base->hms_alarm = RTC_HMS_VAL(tm->tm_hour, tm->tm_min, tm->tm_sec);
	sys_pm_update_rtc_regs();

	/* enable alarm interrupt */
	rtc_acts_set_alarm_interrupt(base, true);

	return 0;
}

static int rtc_acts_set_alarm(struct device *dev, struct rtc_alarm_config *config, bool enable)
{
	struct acts_rtc_data *rtcd = dev->driver_data;
	int ret = 0;

	/* clear old alarm pending */
	rtc_acts_get_pending_int(dev);

	if (enable) {
		if (!config) {
			SYS_LOG_ERR("no alarm configuration");
			return -EINVAL;
		}

		if (rtc_valid_tm(&config->alarm_time)) {
			SYS_LOG_ERR("Bad time structure");
			print_rtc_time(&config->alarm_time);
			return -ENOEXEC;
		}

		rtcd->alarm_cb_fn = config->cb_fn;
		rtcd->cb_data = config->cb_data;
		rtcd->alarm_en = true;
		ret = rtc_acts_set_alarm_time(rtcd, &config->alarm_time);
	} else {
		rtc_acts_set_alarm_interrupt(rtcd->base, false);
		rtcd->alarm_en = false;
	}

	return ret;
}

static int rtc_acts_get_alarm(struct device *dev, struct rtc_alarm_status *sts)
{
	struct acts_rtc_data *rtcd = dev->driver_data;
	struct acts_rtc_controller *base = rtcd->base;
	struct rtc_time *tm = &sts->alarm_time;
	u32_t hms;

	if (!sts)
		return -EINVAL;

	memset(sts, 0, sizeof(struct rtc_alarm_status));

	sts->is_on = rtcd->alarm_en;

	hms = base->hms_alarm;
	tm->tm_sec = RTC_HMS_S(hms);		/* rtc second 0-59 */
	tm->tm_min = RTC_HMS_M(hms);		/* rtc minute 0-59 */
	tm->tm_hour = RTC_HMS_H(hms);		/* rtc hour 0-23 */

	return 0;
}

void rtc_acts_isr(struct device *dev)
{
	struct acts_rtc_data *rtcd = dev->driver_data;
	struct acts_rtc_controller *base = rtcd->base;
	int pending = base->ctrl & RTC_CTL_ALIP;

	SYS_LOG_DBG("enter alarm isr ctl:0x%x", base->ctrl);

	if (pending)
		base->ctrl |= RTC_CTL_ALIP; /* clear pending */

	/* disable alarm irq */
	rtc_acts_set_alarm_interrupt(rtcd->base, false);

	if (pending && rtcd->alarm_en && rtcd->alarm_cb_fn) {
		SYS_LOG_DBG("call alarm_cb_fn %p", rtcd->alarm_cb_fn);
		rtcd->alarm_cb_fn(rtcd->cb_data);
	}
}

static int rtc_acts_check_datetime(struct acts_rtc_data *rtcd)
{
	struct rtc_time tm;
	int ret;

	ret = rtc_acts_get_datetime(rtcd, &tm);
	if (ret)
		return ret;

	if (rtc_valid_tm(&tm) < 0) {
		SYS_LOG_ERR("Invalid RTC date/time, set to default!");

		rtc_time_to_tm(RTC_DEFAULT_INIT_TIME, &tm);
		rtc_acts_set_datetime(rtcd, &tm);
	}

	return 0;
}

static bool rtc_acts_is_alarm_wakeup(struct device *dev)
{
	struct acts_rtc_data *rtcd = dev->driver_data;
	if (rtcd->alarm_wakeup)
		return true;
	else
		return false;
}

const struct rtc_driver_api rtc_acts_driver_api = {
	.enable = rtc_acts_enable,
	.disable = rtc_acts_disable,
	.get_time = rtc_acts_get_time,
	.set_time = rtc_acts_set_time,
	.set_alarm = rtc_acts_set_alarm,
	.get_alarm = rtc_acts_get_alarm,
	.is_alarm_wakeup = rtc_acts_is_alarm_wakeup,
};

int rtc_acts_init(struct device *dev)
{
	const struct acts_rtc_config *cfg = dev->config->config_info;
	struct acts_rtc_data *rtcd = dev->driver_data;
	struct acts_rtc_controller *base = cfg->base;

	rtcd->base = base;
	rtcd->alarm_wakeup = false;

	if (base->ctrl & RTC_CTL_ALIP)
		rtcd->alarm_wakeup = true;

	/* By default to disable RTC alarm IRQ */
	if (base->ctrl & RTC_CTL_ALIE)
		base->ctrl |= RTC_CTL_ALIE;

	rtc_acts_init_clksource(rtcd);

	/* By default to enable the rtc function */
	rtc_acts_enable(dev);

	rtc_acts_check_datetime(rtcd);

	cfg->irq_config();

	SYS_LOG_INF("Actions RTC module initialized successfully");

	return 0;
}

static void rtc_acts_irq_config(void);
struct acts_rtc_data rtc_acts_ddata;

static const struct acts_rtc_config rtc_acts_cdata = {
	.base = (struct acts_rtc_controller *)RTC_REG_BASE,
	.irq_config = rtc_acts_irq_config,
};

DEVICE_AND_API_INIT(rtc_acts, CONFIG_RTC_0_NAME, rtc_acts_init,
		    &rtc_acts_ddata, &rtc_acts_cdata,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &rtc_acts_driver_api);

static void rtc_acts_irq_config(void)
{
	IRQ_CONNECT(IRQ_ID_RTC, CONFIG_IRQ_PRIO_LOW,
		    rtc_acts_isr, DEVICE_GET(rtc_acts), 0);
	irq_enable(IRQ_ID_RTC);
}
