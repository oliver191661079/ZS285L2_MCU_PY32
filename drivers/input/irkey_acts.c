/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief infrared keyboard driver for Actions SoC
 */

#include <errno.h>
#include <kernel.h>
#include <string.h>
#include <init.h>
#include <irq.h>
#include <input_dev.h>
#include <board.h>

#define SYS_LOG_DOMAIN "IR_KEY"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_INPUT_DEV_LEVEL
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_IRKEY_ACTS

#define IRKEY_POLL_RELEASE_MS		200

/* IR registers bits define */

#define IRC_CTL_DBB_EN			(0x1 << 16)
#define IRC_CTL_DBC_MASK		(0xfff << 4)
#define IRC_CTL_DBC_SHIFT		(4)
#define IRC_CTL_DBC(x)			((x) << IRC_CTL_DBC_SHIFT)
#define IRC_CTL_IRE			(0x1 << 3)
#define IRC_CTL_IIE			(0x1 << 2)
#define IRC_CTL_ICMS_MASK		(0x3 << 0)
#define IRC_CTL_ICMS_SHIFT		(0)
#define IRC_CTL_ICMS(x)			((x) << IRC_CTL_DBC_SHIFT)
#define IRC_CTL_ICMS_9012		IRC_CTL_ICMS(0)
#define IRC_CTL_ICMS_8BITS_NEC		IRC_CTL_ICMS(1)
#define IRC_CTL_ICMS_RC5		IRC_CTL_ICMS(2)
#define IRC_CTL_ICMS_RC6		IRC_CTL_ICMS(3)

#define IRC_STA_UCMP			(0x1 << 6)
#define IRC_STA_KDCM			(0x1 << 5)
#define IRC_STA_RCD			(0x1 << 4)
#define IRC_STA_IIP			(0x1 << 2)
#define IRC_STA_IREP			(0x1 << 0)


/* I2C controller */
struct ir_acts_controller {
	volatile uint32_t ctl;
	volatile uint32_t stat;
	volatile uint32_t cc;
	volatile uint32_t kdc;
};

struct acts_irkey_data {
	input_notify_t notify;
	struct k_timer timer;
	int last_key_code;
};

struct irkey_map {
	u16_t key_code;
	u16_t key_val;
};

struct acts_irkey_config {
	struct ir_acts_controller *base;
	int ir_mode;
	int user_code;
	void (*irq_config_func)(struct device *dev);

	int key_cnt;
	const struct irkey_map *key_maps;
};

/* key_data -> key_code */
static u16_t irkey_acts_get_keycode(const struct acts_irkey_config *cfg,
				    int key_data)
{
	const struct irkey_map *map = cfg->key_maps;
	int i;

	for (i = 0; i < cfg->key_cnt; i++) {
		if (key_data == map->key_val)
			return map->key_code;

		map++;
	}

	return KEY_RESERVED;
}

static void irkey_acts_report_key(struct acts_irkey_data *irkey,
				   int key_code, int value)
{
	struct input_value val;

	if (irkey->notify) {
		val.type = EV_KEY;
		val.code = key_code;
		val.value = value;

		SYS_LOG_DBG("report key_code %d value %d",
			key_code, value);

		irkey->notify(NULL, &val);
	}
}

static void irkey_acts_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct acts_irkey_config *cfg = dev->config->config_info;
	struct acts_irkey_data *irkey = dev->driver_data;
	struct ir_acts_controller *irc = cfg->base;
	int key_data, key_code;

	/* clear irq pending */
	irc->stat = IRC_STA_IIP | IRC_STA_RCD;

	key_data = irc->kdc & 0xff;
	key_code = irkey_acts_get_keycode(cfg, key_data);

	SYS_LOG_DBG("key_data 0x%x, key_code 0x%x\n",
		key_data, key_code);

	if (key_code == KEY_RESERVED)
		return;

	/* previous key is released? */
	if (irkey->last_key_code != KEY_RESERVED &&
	    irkey->last_key_code != key_code) {
		irkey_acts_report_key(irkey, irkey->last_key_code, 0);
	}

	irkey_acts_report_key(irkey, key_code, 1);

	irkey->last_key_code = key_code;

	/* poll key is released */
	k_timer_start(&irkey->timer, IRKEY_POLL_RELEASE_MS, 0);
}

static void irkey_acts_poll_release(struct k_timer *timer)
{
	struct device *dev = k_timer_user_data_get(timer);
	struct acts_irkey_data *irkey = dev->driver_data;

	/* previous key is released? */
	if (irkey->last_key_code != KEY_RESERVED) {
		irkey_acts_report_key(irkey, irkey->last_key_code, 0);
	}

	irkey->last_key_code = KEY_RESERVED;
}

static void irkey_acts_enable(struct device *dev)
{
	const struct acts_irkey_config *cfg = dev->config->config_info;
	struct ir_acts_controller *irc = cfg->base;

	ACT_LOG_ID_DBG(ALF_STR_irkey_acts_enable__ENABLE_IRKEY, 0);

	irc->ctl |= IRC_CTL_DBB_EN | IRC_CTL_IIE;
}

static void irkey_acts_disable(struct device *dev)
{
	const struct acts_irkey_config *cfg = dev->config->config_info;
	struct acts_irkey_data *irkey = dev->driver_data;
	struct ir_acts_controller *irc = cfg->base;

	ACT_LOG_ID_DBG(ALF_STR_irkey_acts_disable__DISABLE_IRKEY, 0);

	irc->ctl &= ~(IRC_CTL_DBB_EN | IRC_CTL_IIE);
	k_timer_stop(&irkey->timer);

	irkey->last_key_code = KEY_RESERVED;
}

static void irkey_acts_register_notify(struct device *dev, input_notify_t notify)
{
	struct acts_irkey_data *irkey = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_irkey_acts_register_notify__REGISTER_NOTIFY_0XX, 1, (u32_t)notify);

	irkey->notify = notify;
}

static void irkey_acts_unregister_notify(struct device *dev, input_notify_t notify)
{
	struct acts_irkey_data *irkey = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_irkey_acts_unregister_notify__UNREGISTER_NOTIFY_0X, 1, (u32_t)notify);

	irkey->notify = NULL;
}

static const struct input_dev_driver_api irkey_acts_driver_api = {
	.enable = irkey_acts_enable,
	.disable = irkey_acts_disable,
	.register_notify = irkey_acts_register_notify,
	.unregister_notify = irkey_acts_unregister_notify,
};

static int irkey_acts_init(struct device *dev)
{
	const struct acts_irkey_config *cfg = dev->config->config_info;
	struct acts_irkey_data *irkey = dev->driver_data;
	struct ir_acts_controller *irc = cfg->base;

	/* enable ir controller clock */
	acts_clock_peripheral_enable(CLOCK_ID_IR);

	/* reset ir controller */
	acts_reset_peripheral(RESET_ID_IR);

	/* clear old pendings */
	irc->stat |= 0x0;

	/* init user code */
	irc->cc = cfg->user_code;

	irc->ctl = IRC_CTL_IRE | IRC_CTL_DBB_EN | IRC_CTL_DBC(0x28) | cfg->ir_mode;

	k_timer_init(&irkey->timer, irkey_acts_poll_release, NULL);
	k_timer_user_data_set(&irkey->timer, dev);

	irkey->last_key_code = KEY_RESERVED;

	cfg->irq_config_func(dev);

	return 0;
}

static struct acts_irkey_data irkey_acts_ddata;

static const struct irkey_map irkey_acts_keymaps[] = {
	BOARD_IRKEY_KEY_MAPPINGS
};

static void irkey_acts_irq_config(struct device *dev);

static const struct acts_irkey_config irkey_acts_cdata = {
	.base = (struct ir_acts_controller *)IRC_REG_BASE,
#ifdef BOARD_IRKEY_PROTOCOL
	.ir_mode = BOARD_IRKEY_PROTOCOL,
#else
	.ir_mode = 1,	/* 8bit NEC */
#endif

#ifdef BOARD_IRKEY_PROTOCOL
	.user_code = BOARD_IRKEY_USER_CODE,
#else
	.user_code = 0xff00,
#endif

	.irq_config_func = irkey_acts_irq_config,

	.key_cnt = ARRAY_SIZE(irkey_acts_keymaps),
	.key_maps = &irkey_acts_keymaps[0],
};

DEVICE_AND_API_INIT(irkey_acts, CONFIG_INPUT_DEV_ACTS_IRKEY_NAME,
		    irkey_acts_init,
		    &irkey_acts_ddata, &irkey_acts_cdata,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &irkey_acts_driver_api);

static void irkey_acts_irq_config(struct device *dev)
{
	IRQ_CONNECT(IRQ_ID_IR, CONFIG_IRQ_PRIO_NORMAL, irkey_acts_isr, DEVICE_GET(irkey_acts), 0);
	irq_enable(IRQ_ID_IR);
}