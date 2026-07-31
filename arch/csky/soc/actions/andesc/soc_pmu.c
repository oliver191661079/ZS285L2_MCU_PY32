/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file PMU interface
 */

#include <kernel.h>
#include <soc.h>
#include <soc_pmu.h>
#include <soc_atp.h>

unsigned int soc_pmu_get_vdd_voltage(void)
{
	unsigned int sel, volt_mv;

	sel = (sys_read32(VOUT_CTL) >> 4) & 0xf;

	volt_mv = 800 + sel * 50;

	return volt_mv;
}

void soc_pmu_set_vdd_voltage(unsigned int volt_mv)
{
	unsigned int sel;

	if (volt_mv < 800 || volt_mv > 1500)
		return;

	sel = (volt_mv - 800) / 50;
	sys_write32((sys_read32(VOUT_CTL) & ~(0xf << 4)) | (sel << 4), VOUT_CTL);

	k_busy_wait(1000);
}

void pmu_spll_init(void)
{
	volatile int i;

	/* enable SPLL AVDD */
	sys_write32(sys_read32(VOUT_CTL) | (1 << 20), VOUT_CTL);

	/* wait SPLL AVDD ready */
	for (i = 0; i < 150; i++)
		;

	/* enable SPLL 32MHz clock */
	sys_write32(sys_read32(SPLL_CTL) | 0x11, SPLL_CTL);
}

void pmu_init(void)
{

	sys_write32(0x1de6a4e, DCDC_CTL1);
	sys_write32(0x5865a99b, DCDC_CTL2);

	/* disable OSCVDD pull-down resistor */
	sys_write32((sys_read32(LDO_CTL) & ~0x80), LDO_CTL);

	/* disable AVDD pull-down resistor */
	sys_write32((sys_read32(VOUT_CTL) & ~(1 << 18)), VOUT_CTL);

	/* disable the pull-down resistor of BANDGAP && enable filter resistor */
	sys_write32((sys_read32(BDG_CTL) & ~(1 << 5)) | (1 << 6), BDG_CTL);

	pmu_spll_init();
}

int soc_pmu_lradc_adjust(uint32_t *lradc_data)
{
	static uint32_t lradc_param = 0;
	static u8_t lradc_param_inited = 0;
	int ret_val;
	int gain_float;
	int gainerror;
	int offerror_data;
	int offset_error;

	uint32_t key = irq_lock();
	if (!lradc_param_inited){
		ret_val = soc_get_lradc_adjust_param(&lradc_param);

		if(ret_val){
			irq_unlock(key);
			return ret_val;
		}

		lradc_param_inited = 1;
	}

	irq_unlock(key);

	if(lradc_param == 0){
		return 0;
	}

	gain_float = lradc_param & 0xff;

	if((lradc_param & (1 << 8))){
		gainerror = gain_float + 10000;
	}else{
		gainerror = 10000 - gain_float;
	}

	offerror_data = (lradc_param >> 9) & 0xf;

	if((lradc_param & (1 << 13)) == 0){
		offset_error = offerror_data;
	}else{
		offset_error = -offerror_data;
	}

	ret_val = *lradc_data;

	if(ret_val > offset_error){
		*lradc_data = ((ret_val - offset_error) * 10000) / gainerror;
	}


	//printk("lradc:original val %x offset error %x gainerror %x adjst %x\n", ret_val, offset_error, gainerror, *lradc_data);

	return 0;
}

