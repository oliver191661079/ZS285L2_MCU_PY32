/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef	_ACTIONS_SOC_ATP_H_
#define	_ACTIONS_SOC_ATP_H_

#ifndef _ASMLANGUAGE

extern int soc_atp_read_bits(u32_t *dat, int start_bit, int bit_cnt);
extern unsigned int soc_get_system_info(unsigned int module);
int soc_atp_get_hosc_calib(int id, unsigned char *calib_val);
int soc_atp_set_hosc_calib(int id, unsigned char calib_val);
//uuid is 128bits
int soc_get_system_uuid(unsigned int *uuid);
int soc_get_lradc_adjust_param(uint32_t *val);
#endif /* _ASMLANGUAGE */

#endif /* _ACTIONS_SOC_ATP_H_	*/
