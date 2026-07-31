/*
 * Copyright (c) 1997-2015, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <soc.h>
#include <misc/util.h>
#include <dsp.h>
#include "dsp_inner.h"
#include <logging/sys_log.h>
#include "os_common_api.h"
#define ACT_LOG_MODEL_ID ALF_MODEL_DSP_DEV

#ifdef CONFIG_DSP_DEBUG_PRINT
#include <acts_ringbuf.h>
#endif

/* if set, the user can externally shut down the DSP root clock (gated by CMU) */
#define PSU_DSP_IDLE BIT(3)
/* if set, only the DSP Internal core clock is gated */
#define PSU_DSP_CORE_IDLE BIT(2)

static void dsp_acts_isr(void *arg);

static void dsp_acts_irq_enable(void);

static void dsp_acts_irq_disable(void);


static int get_hw_idle(void)
{
	return sys_read32(CMU_DSP_WAIT) & PSU_DSP_IDLE;
}

int wait_hw_idle_timeout(int usec_to_wait)
{
	do {
		if (get_hw_idle())
			return 0;
		if (usec_to_wait-- <= 0)
			break;
		k_busy_wait(1);
	} while (1);

	return -EAGAIN;
}

#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
static bool dsp_wait_buf_condition_satisfy_flag = 0;
static bool dsp_wait_cpu_allow_flag= 0;
static bool dsp_wait_read_buf_condition_satisfy_flag = 0;
static bool dsp_wait_write_buf_condition_satisfy_flag = 0;
static bool dsp_wait_bt_read_buf_condition_satisfy_flag = 0;

static bool dsp_wait_latency_first = 0;
static bool dsp_wait_change_wait_threshold_lock = 0;

struct dsp_pwr_t dsp_pwr_info_exchange={
    .power_latency = CONFIG_DSP_ACTIVE_POWER_LATENCY_MS,
    .dsp_wait_threshold = CONFIG_DSP_WAIT_THRESHOLD,
    .dsp_work_percent = 0,
    .dsp_stat_period = 8000,//dsp statistics work percent period
};

void dsp_wait_set_latency_prio(bool prio)
{
    printk(" %s %d \n",__func__,prio);
    dsp_wait_latency_first = prio;
    dsp_pwr_info_exchange.cpu_disable_dsp_wait = !prio;
}
int dsp_wait_get_latency_prio(void)
{
    return dsp_wait_latency_first;
}

static void dsp_wait_disable_buf_condition_satisfy(void)
{
	struct device *dsp_dev = device_get_binding(CONFIG_DSP_ACTS_DEV_NAME);

	if(!dsp_dev){
		return;
	}

    dsp_wait_buf_condition_satisfy_flag = false;
    dsp_pwr_info_exchange.cpu_disable_dsp_wait = 1;
    dsp_resume(dsp_dev);
//    printk("%s\n",__func__);
}

static void dsp_wait_enable_buf_condition_satisfy(void)
{
    if(dsp_wait_cpu_allow_flag){
        dsp_wait_buf_condition_satisfy_flag = true;
        dsp_pwr_info_exchange.cpu_disable_dsp_wait = 0;
    }else{
        dsp_wait_buf_condition_satisfy_flag = false;
        dsp_pwr_info_exchange.cpu_disable_dsp_wait = 1;
    }
//    printk("%s\n",__func__);
}
void dsp_wait_forbit_dsp_wait_lock(void)
{
	dsp_wait_change_wait_threshold_lock = 1;
	dsp_pwr_info_exchange.dsp_wait_threshold = 0;
}

void dsp_wait_forbit_dsp_wait_unlock(void)
{
	dsp_wait_change_wait_threshold_lock = 0;
	dsp_pwr_info_exchange.dsp_wait_threshold = CONFIG_DSP_WAIT_THRESHOLD;
}

int dsp_wait_get_buf_condition_satisfy(void)
{
    return dsp_wait_buf_condition_satisfy_flag;
}

void dsp_wait_disable_cpu_allow(void)
{
	struct device *dsp_dev = device_get_binding(CONFIG_DSP_ACTS_DEV_NAME);

	if(!dsp_dev){
		return;
	}

	if(dsp_pwr_info_exchange.dsp_wait_threshold && (dsp_wait_cpu_allow_flag))
		printk("%s: dsp_wait_thd %d%s\n",__func__,dsp_pwr_info_exchange.dsp_wait_threshold,"%");

    dsp_wait_cpu_allow_flag = false;
    dsp_wait_buf_condition_satisfy_flag = false;
	dsp_wait_write_buf_condition_satisfy_flag = false;
	dsp_wait_read_buf_condition_satisfy_flag = false;
	dsp_wait_bt_read_buf_condition_satisfy_flag = false;
    dsp_pwr_info_exchange.cpu_disable_dsp_wait = 1;
    dsp_resume(dsp_dev);
}

void dsp_wait_enable_cpu_allow(void)
{
    if(dsp_wait_cpu_allow_flag == false){
		dsp_wait_cpu_allow_flag = true;
		if(dsp_pwr_info_exchange.dsp_wait_threshold)
			printk("%s: dsp_wait_thd %d%s\n",__func__,dsp_pwr_info_exchange.dsp_wait_threshold,"%");
	}
}

int dsp_wait_cpu_get_allow_flag(void)
{
    return dsp_wait_cpu_allow_flag;
}

void dsp_wait_disable_bt_read_buf_condition_satisfy(void)
{
    unsigned int flags;
    flags = irq_lock();
    dsp_wait_bt_read_buf_condition_satisfy_flag = 0;
    if(dsp_wait_get_buf_condition_satisfy())
	{
        dsp_wait_disable_buf_condition_satisfy();
    }
    irq_unlock(flags);
}

void dsp_wait_enable_bt_read_buf_condition_satisfy(void)
{
    unsigned int flags;
    flags = irq_lock();

    dsp_wait_bt_read_buf_condition_satisfy_flag = 1;
	//if(dsp_wait_get_write_buf_condition_satisfy())
    {
        if((!dsp_wait_get_buf_condition_satisfy() )){
            dsp_wait_enable_buf_condition_satisfy();
        }
    }
    irq_unlock(flags);
}


void dsp_wait_disable_read_buf_condition_satisfy(void)
{
    unsigned int flags;
    flags = irq_lock();

    dsp_wait_read_buf_condition_satisfy_flag = 0;
    if(dsp_wait_get_buf_condition_satisfy())
	{
        dsp_wait_disable_buf_condition_satisfy();
    }
    irq_unlock(flags);
    //printk("%s\n",__func__);

}

void dsp_wait_enable_read_buf_condition_satisfy(void)
{
    unsigned int flags;
    flags = irq_lock();

    dsp_wait_read_buf_condition_satisfy_flag = 1;
    //if(dsp_wait_get_write_buf_condition_satisfy())
    {
        if(!dsp_wait_get_buf_condition_satisfy()){
            dsp_wait_enable_buf_condition_satisfy();
        }
    }
    irq_unlock(flags);
    //printk("%s\n",__func__);
}



void dsp_wait_disable_write_buf_condition_satisfy(void)
{
//    unsigned int flags;
//    flags = irq_lock();
//    dsp_wait_write_buf_condition_satisfy_flag = 0;
//    dsp_wait_disable_buf_condition_satisfy();
//    irq_unlock(flags);
}

void dsp_wait_enable_write_buf_condition_satisfy(void)
{
//    unsigned int flags;
//    flags = irq_lock();
//    dsp_wait_write_buf_condition_satisfy_flag = 1;
//	//if(dsp_wait_read_buf_condition_satisfy_flag && dsp_wait_read_buf_condition_satisfy_flag)
//	{
//        dsp_wait_enable_buf_condition_satisfy();
//    }
//    irq_unlock(flags);
}

int dsp_wait_get_write_buf_condition_satisfy(void)
{
//    return dsp_wait_write_buf_condition_satisfy_flag;
	return 1;
}


int dsp_wait_get_work_percent(void)
{
    return dsp_pwr_info_exchange.dsp_work_percent;
}

int dsp_wait_get_wait_percent(void)
{
    return dsp_pwr_info_exchange.dsp_wait_percent;
}


uint32_t dsp_wait_get_work_threshold(void)
{
    return dsp_pwr_info_exchange.dsp_wait_threshold;
}

void dsp_wait_set_count_enable(int enable)
{
    dsp_pwr_info_exchange.dsp_idle_count_enable = !!enable;
}


int dsp_wait_set_work_threshold(int thd)
{
    if(thd >= 0 && thd < 100){
		if(!dsp_wait_change_wait_threshold_lock){
			dsp_pwr_info_exchange.dsp_wait_threshold = thd;
		}else{
			ACT_LOG_ID_INF(ALF_STR_dsp_wait_set_work_threshold__FAILED__CHANGE_WAIT_, 0);
		}
        return 0;
    }else{
        return -1;
    }
}

int dsp_acts_wait_set_stat_period(int period)
{
    if(period >= 0){
        dsp_pwr_info_exchange.dsp_stat_period = period*1000;
        ACT_LOG_ID_INF(ALF_STR_dsp_acts_wait_set_stat_period__SET_DSP_STAT_PERIOD_, 1,dsp_pwr_info_exchange.dsp_stat_period);
        return 0;
    }else{
        return -1;
    }
}



int dsp_wait_get_cpu_to_dsp_param(void)
{
    ACT_LOG_ID_INF(ALF_STR_dsp_wait_get_cpu_to_dsp_param__DSP_PWR_INFO_EXCHAGE, 1,(uint32_t)(&dsp_pwr_info_exchange.power_latency));
    return addr_cpu2dsp((uint32_t)(&dsp_pwr_info_exchange.power_latency),false);

}


void dsp_wait_cpu_set_dsp_status_wait(int set)
{
    do{
        dsp_pwr_info_exchange.dsp_wait = set;
    }while(dsp_pwr_info_exchange.dsp_wait != set);

    //printk("%s set %d  is %d \n",__func__,set,dsp_pwr_info_exchange.dsp_wait);
}
#endif

void dsp_wake_from_wait(void)
{
#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
    dsp_wait_disable_read_buf_condition_satisfy();
#endif
}



#ifdef CONFIG_DSP_SET_FRE_AS_CPU_IN_WAIT
extern int soc_dvfs_get_process_mhz(int type);
static int dsp_mhz = 0;
#endif
static int dsp_acts_suspend(struct device *dev)
{
	struct dsp_acts_data *dsp_data = dev->driver_data;

	if (dsp_data->pm_status != DSP_STATUS_POWERON && dsp_data->pm_status != DSP_STATUS_POWERON)
		return 0;

	dsp_do_wait();
#ifdef CONFIG_DSP_SET_FRE_AS_CPU_IN_WAIT
	int cpu = soc_dvfs_get_process_mhz(0);
	int dsp = soc_dvfs_get_process_mhz(1);
	if(cpu > 0 && dsp > cpu){
		if(dsp >= 180){
			dsp_mhz = dsp/2;

			soc_freq_set_dsp_half_and_cpu_the_same();
		}

	}
#endif

	dsp_data->pm_status = DSP_STATUS_SUSPENDED;
//	ACT_LOG_ID_INF(ALF_STR_dsp_acts_suspend__DSP__SUSPENDEDN, 0);
	return 0;
}

static int dsp_acts_resume(struct device *dev)
{
	struct dsp_acts_data *dsp_data = dev->driver_data;

	if (dsp_data->pm_status != DSP_STATUS_SUSPENDED)
		return -EIO;
	sys_write32(sys_read32(CMU_SYSCLK) & (~(3 << 4)), CMU_SYSCLK);

#ifdef CONFIG_DSP_SET_FRE_AS_CPU_IN_WAIT
    int cpu = soc_dvfs_get_process_mhz(0);
    int dsp = soc_dvfs_get_process_mhz(1);
	int ahb = soc_dvfs_get_process_mhz(2);
    if(cpu > 0 && dsp > 0 && dsp_mhz != dsp){
        if(dsp < cpu){
            dsp = cpu;
        }

		soc_freq_set_dsp_cpu_only_by_div(dsp, cpu, ahb);
		dsp_mhz = dsp;
    }
#endif

	dsp_data->pm_status = DSP_STATUS_POWERON;
#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
        dsp_wait_cpu_set_dsp_status_wait(0);
#endif
    dsp_undo_wait();
#if 0
	if (dsp_acts_send_message(dev, &message)) {
		ACT_LOG_ID_INF(ALF_STR_dsp_acts_resume__DSP_RESUME_FAILEDN, 0);
		return -EFAULT;
	}
#endif

//	ACT_LOG_ID_INF(ALF_STR_dsp_acts_resume__DSP__RESUMEDN, 0);
	return 0;
}

static int dsp_acts_kick(struct device *dev, u32_t owner, u32_t event, u32_t params)
{
	struct dsp_acts_data *dsp_data = dev->driver_data;
	const struct dsp_acts_config *dsp_cfg = dev->config->config_info;
	struct dsp_protocol_userinfo *dsp_userinfo = dsp_cfg->dsp_userinfo;
	struct dsp_message message = {
		.id = DSP_MSG_KICK,
		.owner = owner,
		.param1 = event,
		.param2 = params,
	};

	if (dsp_data->pm_status != DSP_STATUS_POWERON)
		return -EACCES;

	if (event != DSP_EVENT_TRIGGER) {
		if (dsp_userinfo->task_state != DSP_TASK_SUSPENDED)
			return -EINPROGRESS;
	}

	return dsp_acts_send_message(dev, &message);
}


#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
static int dsp_acts_wait_init(struct device *dev, u32_t owner)
{
    struct dsp_acts_data *dsp_data = dev->driver_data;
    struct dsp_message message = {
        .id = DSP_MSG_WAIT,
        .owner = owner,
        .param1 = (uint32_t)dsp_wait_get_cpu_to_dsp_param(),
    };

    if (dsp_data->pm_status != DSP_STATUS_POWERON)
        return -EACCES;

    int ret = dsp_acts_send_message(dev, &message);
	if(dsp_pwr_info_exchange.dsp_wait_threshold)
		printk("%s  %s\n",__func__,ret?"failed!!!":"successfully !!!!");

    return ret;
}

#endif



static int dsp_acts_power_on(struct device *dev, void *cmdbuf)
{
	struct dsp_acts_data *dsp_data = dev->driver_data;
	const struct dsp_acts_config *dsp_cfg = dev->config->config_info;
	struct dsp_protocol_userinfo *dsp_userinfo = dsp_cfg->dsp_userinfo;
	int i;

	if (dsp_data->pm_status != DSP_STATUS_POWEROFF)
		return 0;

	if (dsp_data->images[DSP_IMAGE_MAIN].size == 0) {
		printk("%s: no image loaded\n", __func__);
		return -EFAULT;
	}

	if (cmdbuf == NULL) {
		printk("%s: must assign a command buffer\n", __func__);
		return -EINVAL;
	}

	/* set bootargs */
	dsp_data->bootargs.command_buffer = addr_cpu2dsp((u32_t)cmdbuf, false);
	dsp_data->bootargs.sub_entry_point = dsp_data->images[1].entry_point;

	/* assert dsp wait signal */
	dsp_do_wait();

	/* assert reset */
	acts_reset_peripheral_assert(RESET_ID_DSP_ALL);

	/* enable dsp clock*/
	acts_clock_peripheral_enable(CLOCK_ID_DSP);
	acts_clock_peripheral_enable(CLOCK_ID_DSP_COMM);

	/* set memery access */
	dsp_soc_request_mem();

	/* intialize shared registers */
	dsp_cfg->dsp_mailbox->msg = MAILBOX_MSG(DSP_MSG_NULL, MSG_FLAG_ACK) ;
	dsp_cfg->dsp_mailbox->owner = 0;
	dsp_cfg->cpu_mailbox->msg = MAILBOX_MSG(DSP_MSG_NULL, MSG_FLAG_ACK) ;
	dsp_cfg->cpu_mailbox->owner = 0;
	dsp_userinfo->task_state = DSP_TASK_PRESTART;
	dsp_userinfo->error_code = DSP_NO_ERROR;

	/* set dsp vector_address */
	//set_dsp_vector_addr((unsigned int)DSP_ADDR);
	//printk("%s: DSP_VCT_ADDR=0x%x 0x%x\n", __func__, sys_read32(DSP_VCT_ADDR), dsp_data->bootargs.sub_entry_point);

	dsp_data->pm_status = DSP_STATUS_PRE_POWERON;

	/* clear all pending */
	clear_dsp_all_irq_pending();

	/* enable dsp irq */
	dsp_acts_irq_enable();

	/* deassert dsp wait signal */
	dsp_undo_wait();

#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
    dsp_wait_disable_cpu_allow();
#endif

	k_sem_reset(&dsp_data->msg_sem);

	/* deassert reset */
	acts_reset_peripheral_deassert(RESET_ID_DSP_ALL);

	/* wait message DSP_MSG_BOOTUP */
	if (k_sem_take(&dsp_data->msg_sem, 100)) {
		printk("dsp image <%s> cannot boot up\n", dsp_data->images[DSP_IMAGE_MAIN].name);
		panic("dsp load error\n");
		goto cleanup;
	}

	/* FIXME: convert userinfo address from dsp to cpu here, since
	 * dsp will never touch it again.
	 */
	dsp_userinfo->func_table = addr_dsp2cpu(dsp_userinfo->func_table, false);
	for (i = 0; i < dsp_userinfo->func_size; i++) {
		volatile u32_t addr = dsp_userinfo->func_table + i * 4;
		if (addr > 0)   /* NULL, no information provided */
			sys_write32(addr_dsp2cpu(sys_read32(addr), false), addr);
	}

	//ACT_LOG_ID_INF(ALF_STR_dsp_acts_power_on__DSP_POWER_ON_D_08XN, 2, dsp_userinfo->func_size, dsp_userinfo->func_table);
	dsp_data->pm_status = DSP_STATUS_POWERON;

#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
    dsp_acts_wait_init(dev,0);
#endif

	return 0;
cleanup:
	dsp_acts_irq_disable();
	acts_clock_peripheral_disable(CLOCK_ID_DSP);
	acts_reset_peripheral_assert(RESET_ID_DSP_ALL);
	return -ETIMEDOUT;
}

static int dsp_acts_power_off(struct device *dev)
{
	const struct dsp_acts_config *dsp_cfg = dev->config->config_info;
	struct dsp_acts_data *dsp_data = dev->driver_data;

	if (dsp_data->pm_status == DSP_STATUS_POWEROFF)
		return 0;

#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
        dsp_wait_enable_cpu_allow();
#endif


	/* assert dsp wait signal */
	dsp_do_wait();

	/* disable dsp irq */
	dsp_acts_irq_disable();

	/* disable dsp clock*/
	acts_clock_peripheral_disable(CLOCK_ID_DSP);

	/* assert reset dsp module */
	acts_reset_peripheral_assert(RESET_ID_DSP_ALL);

	/* deassert dsp wait signal */
	dsp_undo_wait();

	/* clear page mapping */
	clear_dsp_pageaddr();

	dsp_cfg->dsp_userinfo->task_state = DSP_TASK_DEAD;
	dsp_data->pm_status = DSP_STATUS_POWEROFF;

	ACT_LOG_ID_INF(ALF_STR_dsp_acts_power_off__DSP_POWER_OFFN, 0);
	return 0;
}

static int acts_request_userinfo(struct device *dev, int request, void *info)
{
	const struct dsp_acts_config *dsp_cfg = dev->config->config_info;
	struct dsp_protocol_userinfo *dsp_userinfo = dsp_cfg->dsp_userinfo;
	union {
		struct dsp_request_session *ssn;
		struct dsp_request_function *func;
	} req_info;

	switch (request) {
	case DSP_REQUEST_TASK_STATE:
		*(int *)info = dsp_userinfo->task_state;
		break;
	case DSP_REQUEST_ERROR_CODE:
		*(int *)info = dsp_userinfo->error_code;
		break;
	case DSP_REQUEST_SESSION_INFO:
		req_info.ssn = info;
		req_info.ssn->func_enabled = dsp_userinfo->func_enabled;
		req_info.ssn->func_runnable = dsp_userinfo->func_runnable;
		req_info.ssn->func_counter = dsp_userinfo->func_counter;
		req_info.ssn->info = (void *)addr_dsp2cpu(dsp_userinfo->ssn_info, false);
		break;
	case DSP_REQUEST_FUNCTION_INFO:
		req_info.func = info;
		if (req_info.func->id < dsp_userinfo->func_size)
			req_info.func->info = (void *)sys_read32(dsp_userinfo->func_table + req_info.func->id * 4);
		break;
	case DSP_REQUEST_USER_DEFINED:
		{
            uint32_t index = *(uint32_t*)info;
            uint32_t debug_info = *(volatile uint32_t*)(DSP_DEBUG_REGION_REGISTER_BASE + 4 * index);
            *(uint32_t*)info = debug_info;
            break;
    	}
	default:
		return -EINVAL;
	}

	return 0;
}

static void dsp_acts_isr(void *arg)
{
	/* clear irq pending bits */
	clear_dsp_irq_pending(IRQ_ID_OUT_USER0);

	dsp_acts_recv_message(arg);
}



static int dsp_acts_init(struct device *dev)
{
	const struct dsp_acts_config *dsp_cfg = dev->config->config_info;
	struct dsp_acts_data *dsp_data = dev->driver_data;

#ifdef CONFIG_DSP_DEBUG_PRINT
	dsp_data->bootargs.debug_buf = mcu_to_dsp_data_address(POINTER_TO_UINT(acts_ringbuf_alloc(0x100)));
#endif

	dsp_cfg->dsp_userinfo->task_state = DSP_TASK_DEAD;
	k_sem_init(&dsp_data->msg_sem, 0, 1);
	k_mutex_init(&dsp_data->msg_mutex);
	return 0;
}

static struct dsp_acts_data dsp_acts_data = {
	.bootargs.power_latency = CONFIG_DSP_ACTIVE_POWER_LATENCY_MS,
	.bootargs.debug_buf = 0,
};

static const struct dsp_acts_config dsp_acts_config = {
	.dsp_mailbox = (struct dsp_protocol_mailbox *)DSP_M2D_MAILBOX_REGISTER_BASE,
	.cpu_mailbox = (struct dsp_protocol_mailbox *)DSP_D2M_MAILBOX_REGISTER_BASE,
	.dsp_userinfo = (struct dsp_protocol_userinfo *)DSP_USER_REGION_REGISTER_BASE,
};

const struct dsp_driver_api dsp_drv_api = {
	.poweron = dsp_acts_power_on,
	.poweroff = dsp_acts_power_off,
	.suspend = dsp_acts_suspend,
	.resume = dsp_acts_resume,
	.kick = dsp_acts_kick,
	.register_message_handler = dsp_acts_register_message_handler,
	.unregister_message_handler = dsp_acts_unregister_message_handler,
	.request_image = dsp_acts_request_image,
	.release_image = dsp_acts_release_image,
	.send_message = dsp_acts_send_message,
	.request_userinfo = acts_request_userinfo,
};

DEVICE_AND_API_INIT(dsp_acts, CONFIG_DSP_ACTS_DEV_NAME,
		dsp_acts_init, &dsp_acts_data, &dsp_acts_config,
		POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &dsp_drv_api);

void dsp_acts_irq_enable(void)
{
	IRQ_CONNECT(IRQ_ID_OUT_USER0, CONFIG_IRQ_PRIO_HIGH, dsp_acts_isr, DEVICE_GET(dsp_acts), 0);
	irq_enable(IRQ_ID_OUT_USER0);
}

void dsp_acts_irq_disable(void)
{
	irq_disable(IRQ_ID_OUT_USER0);
}




#ifdef CONFIG_DSP_DEBUG_PRINT
#define DEV_DATA(dev) \
	((struct dsp_acts_data * const)(dev)->driver_data)

struct acts_ringbuf *dsp_dev_get_print_buffer(void)
{
	struct dsp_acts_data *data = DEV_DATA(DEVICE_GET(dsp_acts));

	if(!data->bootargs.debug_buf){
		return NULL;
	}

	return (struct acts_ringbuf *)dsp_data_to_mcu_address(data->bootargs.debug_buf);
}
#endif
