/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file pinmux interface for Actions SoC
 */

#ifndef	_ACTIONS_SOC_PINMUX_H_
#define	_ACTIONS_SOC_PINMUX_H_

#ifndef _ASMLANGUAGE

struct acts_pin_config {
	unsigned int pin_num;
	unsigned int mode;
};

int acts_pinmux_set(unsigned int pin, unsigned int mode);
int acts_pinmux_get(unsigned int pin, unsigned int *mode);
void acts_pinmux_setup_pins(const struct acts_pin_config *pinconf, int pins);
void rom_pinmux_setup_pins(const struct acts_pin_config *pinconf, int pins);

#endif /* !_ASMLANGUAGE */

#endif /* _ACTIONS_SOC_PINMUX_H_	*/
