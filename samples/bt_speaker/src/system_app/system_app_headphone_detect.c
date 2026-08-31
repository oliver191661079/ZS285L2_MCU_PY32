/*
 * Copyright (c) 2026 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * GPIO4 耳机插入检测（数字输入 + 内部上拉）：
 *   未插入：上拉为高；插入：检测脚对地，读低。
 *   行为：插入耳机 -> 静音喇叭(ACM8635 0x04=0x0e)，音乐继续(耳机独立出声)；
 *         拔出耳机 -> 恢复喇叭播放(0x04=0x03)。
 *   每 2s 打印 gpio4 level/ctl 供排查 pinmux 被覆盖问题。
 */

#include <zephyr.h>
#include <device.h>
#include <soc.h>
#include <board.h>
#include <amp.h>

#define SYS_LOG_DOMAIN "hp_detect"
#include <logging/sys_log.h>

#ifndef BOARD_285L2_HP_DETECT_GPIO
#define BOARD_285L2_HP_DETECT_GPIO 4
#endif

#define HP_DETECT_POLL_MS		100
#define HP_DETECT_LOG_INTERVAL_MS	2000

/* ACM8635 控制寄存器（page 0） */
#define HP_ACM8635_PAGE_REG		0x00
#define HP_ACM8635_PAGE0		0x00
#define HP_ACM8635_CTRL_REG		0x04
#define HP_ACM8635_CTRL_MUTE	0x0e
#define HP_ACM8635_CTRL_PLAY	0x03

static struct k_delayed_work hp_detect_work;
static bool hp_prev_inserted;
static u32_t hp_last_log_ms;

static void hp_gpio4_configure(void)
{
	/* 整字写 CTL，避免 OUTEN/MFP 残留（同 UART1 TX 修复思路） */
	sys_write32(GPIO_CTL_MFP_GPIO | GPIO_CTL_GPIO_INEN | GPIO_CTL_PULLUP |
		    GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0),
		    GPIO_CTL(BOARD_285L2_HP_DETECT_GPIO));
}

static u32_t hp_gpio4_read_ctl(void)
{
	return sys_read32(GPIO_CTL(BOARD_285L2_HP_DETECT_GPIO));
}

static bool hp_gpio4_read_level(void)
{
	u32_t idat = sys_read32(GPIO_REG_IDAT(GPIO_REG_BASE,
					      BOARD_285L2_HP_DETECT_GPIO));

	return !!(idat & GPIO_BIT(BOARD_285L2_HP_DETECT_GPIO));
}

static bool hp_gpio4_ctl_valid(u32_t ctl)
{
	if (ctl & GPIO_CTL_GPIO_OUTEN) {
		return false;
	}
	if (!(ctl & GPIO_CTL_GPIO_INEN)) {
		return false;
	}
	if (!(ctl & GPIO_CTL_PULLUP)) {
		return false;
	}
	if ((ctl & GPIO_CTL_MFP_MASK) != GPIO_CTL_MFP_GPIO) {
		return false;
	}
	return true;
}

static void hp_gpio4_ensure_config(void)
{
	u32_t ctl = hp_gpio4_read_ctl();

	if (!hp_gpio4_ctl_valid(ctl)) {
		SYS_LOG_WRN("gpio%d ctl=0x%08x invalid, re-init",
			    BOARD_285L2_HP_DETECT_GPIO, (unsigned)ctl);
		hp_gpio4_configure();
	}
}

static bool hp_is_inserted(void)
{
	/* 插入时对地 -> 低电平 */
	return !hp_gpio4_read_level();
}

static void hp_apply_amp_state(bool inserted)
{
	struct device *amp_dev = device_get_binding(CONFIG_AMP_DEV_NAME);

	if (amp_dev == NULL) {
		SYS_LOG_ERR("amp dev(%s) not found", CONFIG_AMP_DEV_NAME);
		return;
	}

	amp_set_reg(amp_dev, HP_ACM8635_PAGE_REG, HP_ACM8635_PAGE0);
	if (inserted) {
		amp_set_reg(amp_dev, HP_ACM8635_CTRL_REG, HP_ACM8635_CTRL_MUTE);
		SYS_LOG_INF("headphone inserted: speaker muted");
	} else {
		amp_set_reg(amp_dev, HP_ACM8635_CTRL_REG, HP_ACM8635_CTRL_PLAY);
		SYS_LOG_INF("headphone removed: speaker playing");
	}
}

static void hp_log_gpio4_status(u32_t now, bool inserted, bool force)
{
	if (!force && (now - hp_last_log_ms) < HP_DETECT_LOG_INTERVAL_MS) {
		return;
	}

	hp_last_log_ms = now;
	SYS_LOG_INF("gpio%d level=%u ctl=0x%08x %s",
		    BOARD_285L2_HP_DETECT_GPIO,
		    (unsigned)hp_gpio4_read_level(),
		    (unsigned)hp_gpio4_read_ctl(),
		    inserted ? "headphone in" : "no headphone");
}

static void hp_detect_work_handler(struct k_work *work)
{
	u32_t now = k_uptime_get_32();
	bool inserted;

	hp_gpio4_ensure_config();
	inserted = hp_is_inserted();

	if (inserted != hp_prev_inserted) {
		hp_prev_inserted = inserted;
		hp_apply_amp_state(inserted);
		hp_log_gpio4_status(now, inserted, true);
	} else {
		hp_log_gpio4_status(now, inserted, false);
	}

	k_delayed_work_submit(&hp_detect_work, K_MSEC(HP_DETECT_POLL_MS));
}

void system_app_headphone_detect_init(void)
{
	hp_gpio4_configure();

	hp_prev_inserted = hp_is_inserted();
	hp_last_log_ms = k_uptime_get_32();
	k_delayed_work_init(&hp_detect_work, hp_detect_work_handler);
	k_delayed_work_submit(&hp_detect_work, K_MSEC(HP_DETECT_POLL_MS));

	SYS_LOG_INF("hp detect init gpio%d level=%u ctl=0x%08x %s",
		    BOARD_285L2_HP_DETECT_GPIO,
		    (unsigned)hp_gpio4_read_level(),
		    (unsigned)hp_gpio4_read_ctl(),
		    hp_prev_inserted ? "headphone in" : "no headphone");
}
