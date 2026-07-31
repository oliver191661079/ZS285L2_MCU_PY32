/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2019-4-28-上午10:46:56             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_sleep.c
 * \brief
 * \author
 * \version  1.0
 * \date  2019-4-28-上午10:46:56
 *******************************************************************************/
#include <kernel.h>
#include <device.h>
#include <init.h>
#include <watchdog.h>
#include <misc/printk.h>
#include <soc.h>

#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SOC_SLEEP
#ifdef CONFIG_MEMORY
#include <mem_manager.h>
#endif

#ifdef CONFIG_PM_SLEEP_TIME_TRACE
static u32_t deep_sleep_start_time;
static u32_t light_sleep_start_time;
static u32_t light_sleep_total_time;
#endif

static uint32_t *s2_reg_backup_ptr;
static uint8_t corepll_backup;
static u32_t deep_sleep_total_time;

extern void trace_set_panic(void);
extern void trace_clear_panic(void);

typedef void (*nor_switch_work_mode)(uint32_t work_mode);
typedef void (*nor_send_command)(uint32_t cmd, uint32_t release_bus);

static const u32_t backup_regs_addr[] = {
	CMU_SPICLK,
	CMU_SYSCLK,
	PMUADC_CTL,
	SPD_CTL,
	AUDIO_PLL1_CTL,
	AUDIO_PLL0_CTL,
	CMU_DEVCLKEN1,
	CMU_DEVCLKEN0,
	RMU_MRCR1,
	RMU_MRCR0,
	LDO_CTL,
	MULTI_USED,
	CMU_MEMCLKEN,
	TIMER_CTL,
	BDG_CTL,
};

static const uint8_t deep_sleep_enable_clock_id[] =
{
    CLOCK_ID_UART0,
    CLOCK_ID_DMA,
    CLOCK_ID_TIMER0,
    CLOCK_ID_SPI0_CACHE,
    CLOCK_ID_SPI0,
#ifdef CONFIG_SOC_PSRAM_DCACHE
    CLOCK_ID_SPI1_CACHE,
    CLOCK_ID_SPI1,
#endif
    CLOCK_ID_BT_BB_3K2,
    CLOCK_ID_BT_BB_DIG,
    CLOCK_ID_BT_BB_AHB,
    CLOCK_ID_BT_MODEM_AHB,
    CLOCK_ID_BT_MODEM_DIG,
    CLOCK_ID_BT_MODEM_INTF,
    CLOCK_ID_BT_RF,
    CLOCK_ID_MCPU,
};

static void pm_backup_registers(uint32_t *backup_buffer)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(backup_regs_addr); i++) {
		backup_buffer[i] = sys_read32(backup_regs_addr[i]);
		//ACT_LOG_ID_INF(ALF_STR_pm_backup_registers__BACKUP_X_XN, 2, backup_regs_addr[i], backup_regs_value[i]);
	}
}

static void pm_restore_registers(uint32_t *backup_buffer)
{
	int i;

	for (i = ARRAY_SIZE(backup_regs_addr) - 1; i >= 0; i--) {
		//ACT_LOG_ID_INF(ALF_STR_pm_restore_registers__RESTORE_X_XN, 2, backup_regs_addr[i], backup_regs_value[i]);
		sys_write32(backup_buffer[i], backup_regs_addr[i]);
	}
}


#ifndef CONFIG_SOC_SERIES_ANDESC
/*
**  检查是否从S3BT 唤醒，及唤醒源
*/
int __ramfunc sys_pm_enter_s3_and_check_wake_up_from_s3bt(void)
{
    int wakeup_pending;

    /* S1-->S3 */
    sys_write32(0x2, PMU_POWER_CTL);

    while(1){
        wakeup_pending = sys_pm_get_wakeup_pending();

        if (wakeup_pending) {
            break;
        }
    }

    return 0;
}
#endif

static uint32_t soc_cmu_deep_sleep(void)
{
	uint32_t spll_backup;

    spll_backup = sys_read32(SPLL_CTL);

    // close dev clk except bt, spi controller
    acts_deep_sleep_clock_peripheral(deep_sleep_enable_clock_id, ARRAY_SIZE(deep_sleep_enable_clock_id));

    /* switch spi0 clock to ck24M */
    sys_write32((sys_read32(CMU_SPICLK) & ~0xff) | 0x40, CMU_SPICLK);

#ifdef CONFIG_SOC_PSRAM_DCACHE
    /* switch spi1 clock to ck24M */
	sys_write32((sys_read32(CMU_SPICLK) & ~0xff00) | 0x4000, CMU_SPICLK);
#endif

    /* switch cpu to CK24M */
    sys_write32((sys_read32(CMU_SYSCLK) & ~(0xf << 8)) | (0xf << 8), CMU_SYSCLK);
    sys_write32((sys_read32(CMU_SYSCLK) & ~0x7) | 0x3, CMU_SYSCLK);

    /* disable hardware memory clock */
    //sys_write32(sys_read32(CMU_MEMCLKEN) & ~(0xff << 24), CMU_MEMCLKEN);
#ifdef CONFIG_SOC_PSRAM_DCACHE
	//sys_write32(sys_read32(CMU_MEMCLKEN) & (~(0xeff7f800)), CMU_MEMCLKEN);
#else
    //sys_write32(sys_read32(CMU_MEMCLKEN) & (~(0xfff7f800)), CMU_MEMCLKEN);
#endif
    sys_write32(0, SPLL_CTL);

    return spll_backup;
}

static void soc_cmu_deep_wakeup(uint32_t spll_backup)
{
    /* restore spll */
    sys_write32(spll_backup, SPLL_CTL);
    k_busy_wait(200);
}

static uint32_t soc_pmu_deep_sleep(void)
{
	uint32_t vout_ctl_backup;

    vout_ctl_backup = sys_read32(VOUT_CTL);

    /* disable adc */
    sys_write32(0, PMUADC_CTL);

    sys_write32(0, SPD_CTL);

	sys_write32(0, MULTI_USED);

	sys_write32(0, TIMER_CTL);

	sys_write32(sys_read32(BDG_CTL) & 0x19f, BDG_CTL);

    /* disable AVDD PULL DOWN, & AVDD =1.0V */
    sys_write32(sys_read32(VOUT_CTL) & (0xffe8ffff), VOUT_CTL);

    return vout_ctl_backup;
}

static void soc_pmu_deep_wakeup(uint32_t vout_ctl_backup)
{
    sys_write32(vout_ctl_backup, VOUT_CTL);
}
extern void soc_pm_deep_power_ctrl(void);

#define MAX_RC_TIMESTAMP (83886079)

static uint32_t soc_get_rc_timestamp_us(uint32_t cur_time, uint32_t prev_time)
{
	if(cur_time > prev_time){
		return cur_time - prev_time;
	}else{
		if(prev_time > MAX_RC_TIMESTAMP){
			prev_time = MAX_RC_TIMESTAMP;
		}

		return (MAX_RC_TIMESTAMP - prev_time + cur_time);
	}
}

void sys_pm_deep_sleep_routinue(void)
{
	uint32_t backup_reg[ARRAY_SIZE(backup_regs_addr)];

	uint32_t spll_backup;

	uint32_t vout_ctl_backup;

	uint32_t deep_sleep_time;

	uint32_t cur_time;

	pm_backup_registers(backup_reg);

    spll_backup = soc_cmu_deep_sleep();

    vout_ctl_backup = soc_pmu_deep_sleep();

    k_busy_wait(300);

	deep_sleep_time = sys_pm_get_rc_timestamp();

#ifdef CONFIG_PM_SLEEP_TIME_TRACE
	deep_sleep_start_time = deep_sleep_time;

    if(light_sleep_start_time){
        light_sleep_total_time += soc_get_rc_timestamp_us(deep_sleep_start_time, light_sleep_start_time);
    }
#endif

#ifndef CONFIG_SOC_SERIES_ANDESC
	sys_pm_enter_s3_and_check_wake_up_from_s3bt();
#else
	soc_pm_deep_power_ctrl();
#endif

	cur_time = sys_pm_get_rc_timestamp();

	deep_sleep_time = soc_get_rc_timestamp_us(cur_time, deep_sleep_time);

	/* compensate ticks with sleep time */
	_sys_clock_tick_count += _ms_to_ticks(deep_sleep_time);

	deep_sleep_total_time += deep_sleep_time;

#ifdef CONFIG_PM_SLEEP_TIME_TRACE

    light_sleep_start_time = sys_pm_get_rc_timestamp();
#endif

    soc_pmu_deep_wakeup(vout_ctl_backup);

    soc_cmu_deep_wakeup(spll_backup);

    pm_restore_registers(backup_reg);
}

/*
**  系统进入s3bt
*/
int sys_pm_enter_deep_sleep(void)
{
	uint32_t key;
	int wakeup_src;

	key = irq_lock();

	/* enable sychronized print */
	//trace_set_panic();

	sys_pm_set_wakeup_src(1);
	sys_pm_clear_wakeup_pending();

	//printk("[S3BT] WKEN_CTL: %x, WK_PD %x\n",
	//	sys_read32(PMU_WKEN_CTL), sys_read32(PMU_WAKE_PD));

    sys_pm_deep_sleep_routinue();

	wakeup_src = sys_pm_get_wakeup_pending();

	/* clear sychronized print */
	//trace_clear_panic();

	irq_unlock(key);

	return wakeup_src;
}


static void soc_pmu_light_sleep(void)
{
    /* disable adc */
    sys_write32(0, PMUADC_CTL);

    /* disable OSCVDD PULL DOWN */
    sys_write32(sys_read32(LDO_CTL) & (0x0f7f), LDO_CTL);
}

static uint32_t soc_cmu_light_sleep(void)
{
    uint32_t corepll_backup;

    sys_write32(0, AUDIO_PLL0_CTL);
    sys_write32(0, AUDIO_PLL1_CTL);

    /* switch spi0 clock to ck24M */
    sys_write32((sys_read32(CMU_SPICLK) & ~0xff) | 0xc0, CMU_SPICLK);

	/* disable dsp clock */
	sys_write32(sys_read32(CMU_DEVCLKEN1) & ~((1 << 28) | (1 << 29)), CMU_DEVCLKEN1);

    /* disable dsp memory clock */
    //sys_write32(sys_read32(CMU_MEMCLKEN) & ~(0x3 << 7), CMU_MEMCLKEN);

    /* switch cpu to 56M */
    sys_write32((sys_read32(CMU_SYSCLK) & ~0x7) | 0x4, CMU_SYSCLK);
    sys_write32((sys_read32(CMU_SYSCLK) & ~(0xf << 8)) | (0xd << 8), CMU_SYSCLK);

    /* disable coreplls */
    corepll_backup = sys_read32(CMU_COREPLL_CTL);
    sys_write32(sys_read32(CMU_COREPLL_CTL) & ~0x80, CMU_COREPLL_CTL);
    sys_write32(0, CORE_PLL_CTL);

    /* 关闭corepll后修改HGMC change HGMC to 31 */
	sys_write32((sys_read32(CMU_HOSC_CTL) & 0xfffff800) | 0x7d1, CMU_HOSC_CTL);


#ifdef CONFIG_SOC_SERIES_ANDESC
	//disable ram1~7 for dsp used disable hardware memory
	//sys_write32(sys_read32(CMU_MEMCLKEN) & (~0x1fc), CMU_MEMCLKEN);
	/* RAM0 ~ RAM2 are used by BT controller */
	//sys_write32(sys_read32(CMU_MEMCLKEN) & (~0x1f0), CMU_MEMCLKEN);
	//sys_write32(sys_read32(CMU_MEMCLKEN) & (~0x09fff800), CMU_MEMCLKEN);
#endif

	k_busy_wait(100);

    return corepll_backup;
}

static void soc_cmu_ligth_wakeup(uint32_t corepll_backup)
{
    /* 打开前修改HGMC change HGMC to 7 */
    sys_write32((sys_read32(CMU_HOSC_CTL) & 0xfffff800) | 0x131, CMU_HOSC_CTL);

    /* restore core pll */
    sys_write32(corepll_backup, CMU_COREPLL_CTL);
    k_busy_wait(100);
}

void sys_pm_enter_light_sleep(void)
{
	uint32_t key;

	key = irq_lock();

    if(!s2_reg_backup_ptr){
	#ifdef CONFIG_MEMORY
        s2_reg_backup_ptr = (uint32_t *)mem_malloc(sizeof(backup_regs_addr));
    #endif
        if(!s2_reg_backup_ptr){
            irq_unlock(key);
            return;
        }

        pm_backup_registers(s2_reg_backup_ptr);
    }

    soc_pmu_light_sleep();

    corepll_backup = soc_cmu_light_sleep();

    irq_unlock(key);
}

void sys_pm_exit_light_sleep(void)
{
    if(!s2_reg_backup_ptr){
        return;
    }

    soc_cmu_ligth_wakeup(corepll_backup);

    pm_restore_registers(s2_reg_backup_ptr);

#ifdef CONFIG_MEMORY
    mem_free(s2_reg_backup_ptr);
#endif

    s2_reg_backup_ptr = NULL;

    return;
}

#ifdef CONFIG_PM_SLEEP_TIME_TRACE
int sys_pm_get_deep_sleep_time(void)
{
	return deep_sleep_total_time;
}

int sys_pm_get_light_sleep_time(void)
{
	return light_sleep_total_time;
}

#endif

