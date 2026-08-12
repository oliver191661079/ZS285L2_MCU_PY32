/*
 * Copyright (c) 2026 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * WIO0 耳机插入检测（LRADC1 ADC 阈值判定）：
 *   硬件实测：插入耳机 = 3.3V(高ADC)，不接耳机 = 1.6V(低ADC)；脚位 WIO0(LRADC1)。
 *   行为：插入耳机 -> 静音喇叭(ACM8635 0x04=0x0e)，音乐继续(耳机独立出声)；
 *         拔出耳机 -> 恢复喇叭播放(0x04=0x03)。
 *   阈值：10bit ADC，插入/不接中间取阈值，日志会打印 [hp_detect] adc=xxx 供实测调整。
 */

#include <zephyr.h>
#include <device.h>
#include <soc.h>
#include <board.h>
#include <amp.h>

#define SYS_LOG_DOMAIN "hp_detect"
#include <logging/sys_log.h>

/* LRADC1 10bit ADC 阈值：插入耳机=3.3V(高ADC)，不接耳机=1.6V(低ADC)。
 * 参考电压未知，默认 700(约 2.2V)；按日志 adc 实测值调整。 */
#define HP_DETECT_ADC_THRESHOLD		700

/* ACM8635 控制寄存器（page 0） */
#define HP_ACM8635_PAGE_REG		0x00
#define HP_ACM8635_PAGE0		0x00
#define HP_ACM8635_CTRL_REG		0x04
#define HP_ACM8635_CTRL_MUTE	0x0e
#define HP_ACM8635_CTRL_PLAY	0x03

static struct k_delayed_work hp_detect_work;
static bool hp_prev_inserted;

static bool hp_is_inserted(void)
{
	/* LRADC1 数据：10bit，插入(3.3V) 高于阈值，不接(1.6V) 低于阈值 */
	return (sys_read32(LRADC1_DATA) & LRADC1_DATA_LRADC1_MASK) > HP_DETECT_ADC_THRESHOLD;
}

static void hp_apply_amp_state(bool inserted)
{
	struct device *amp_dev = device_get_binding(CONFIG_AMP_DEV_NAME);

	if (amp_dev == NULL) {
		SYS_LOG_ERR("amp dev(%s) not found", CONFIG_AMP_DEV_NAME);
		return;
	}

	/* 先切回 page0，再写 0x04 控制喇叭功放 */
	amp_set_reg(amp_dev, HP_ACM8635_PAGE_REG, HP_ACM8635_PAGE0);
	if (inserted) {
		amp_set_reg(amp_dev, HP_ACM8635_CTRL_REG, HP_ACM8635_CTRL_MUTE);
		SYS_LOG_INF("headphone inserted: speaker muted");
	} else {
		amp_set_reg(amp_dev, HP_ACM8635_CTRL_REG, HP_ACM8635_CTRL_PLAY);
		SYS_LOG_INF("headphone removed: speaker playing");
	}
}

static void hp_detect_work_handler(struct k_work *work)
{
	u32_t adc = sys_read32(LRADC1_DATA) & LRADC1_DATA_LRADC1_MASK;
	bool inserted = adc > HP_DETECT_ADC_THRESHOLD;

	if (inserted != hp_prev_inserted) {
		hp_prev_inserted = inserted;
		hp_apply_amp_state(inserted);
		SYS_LOG_INF("adc=%u -> %s", adc,
			    inserted ? "headphone in" : "no headphone");
	}

	k_delayed_work_submit(&hp_detect_work, K_MSEC(100));
}

void system_app_headphone_detect_init(void)
{
	u32_t ctl;

	/* 使能 LRADC1 通道与时钟 */
	acts_clock_peripheral_enable(CLOCK_ID_LRADC);
	ctl = sys_read32(PMUADC_CTL);
	ctl |= BIT(PMUADC_CTL_LRADC1_EN);
	sys_write32(ctl, PMUADC_CTL);
	k_busy_wait(20);

	hp_prev_inserted = hp_is_inserted();
	k_delayed_work_init(&hp_detect_work, hp_detect_work_handler);
	k_delayed_work_submit(&hp_detect_work, K_MSEC(200));
	SYS_LOG_INF("hp detect init: adc=%u %s",
		    sys_read32(LRADC1_DATA) & LRADC1_DATA_LRADC1_MASK,
		    hp_prev_inserted ? "headphone in" : "no headphone");
}
