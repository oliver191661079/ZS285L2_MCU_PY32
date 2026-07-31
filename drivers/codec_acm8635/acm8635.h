/*
 * Copyright (c) 2026 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ACM8635_H__
#define __ACM8635_H__

#include <zephyr/types.h>

#define ACM8635_I2C_ADDR_DEFAULT	0x38U

#define ACM8635_CHIP_ID_FC		0x86U
#define ACM8635_CHIP_ID_FD		0x25U
#define ACM8635_CHIP_ID_FE		0x53U

struct acm8635_reg_tab {
	u16_t address;
	u8_t data;
};

extern const struct acm8635_reg_tab acm8635_reg_tab_init[];
extern const u32_t acm8635_reg_tab_init_count;
extern const struct acm8635_reg_tab acm8635_reg_tab_main[];
extern const u32_t acm8635_reg_tab_main_count;
extern const u8_t acm8635_vol_table[];
extern const u32_t acm8635_vol_table_len;

#endif /* __ACM8635_H__ */
