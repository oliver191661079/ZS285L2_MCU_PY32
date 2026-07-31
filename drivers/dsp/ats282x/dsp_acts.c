/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief DSP driver for Actions SoC
 */
#include <errno.h>
#include <kernel.h>
#include <string.h>
#include <init.h>
#include <misc/util.h>
#include <misc/byteorder.h>
#include <adc.h>
#include <soc.h>
#ifdef CONFIG_DSP_LIB_IN_SDFS
#include <sdfs.h>
#endif
#include "dsp_leagcy.h"
#include "dsp_inner.h"
#include "sdfs.h"

#define SYS_LOG_DOMAIN "DSP"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_WARNING
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_DSP_ACTS

//extern const struct dsp_lib_info dsp_libs[];
//extern const int dsp_libs_cnt;

/* dma channel registers */
struct acts_dsp_controller {
	volatile u32_t ctrl;
};

struct acts_dsp_config {
	u32_t base;
	u8_t clock_id;
};

struct acts_dsp_data {
	u32_t enable_cnt;
};

struct dsp dsps[2];

struct dsp_comm * const dsp_comm = (struct dsp_comm *)DSP_IRQ_VECTOR_ADDR;

void (*dsp_irq_callback)(void);

int respond_dsp(void)
{
	int count = 0x60000;
	//set m4k's cmd
	dsp_comm->mcu_to_dsp.state = CPU_COMM_CMD_EXIST;
	//clear dsp'cmd
	if (dsp_comm->dsp_to_mcu.state != CPU_COMM_CMD_DEAL)
	{
          //插入wait信号
		  dsp_do_wait();
          ACT_LOG_ID_INF(ALF_STR_respond_dsp__150_DSP_STATEXN, 1, dsp_comm->dsp_to_mcu.state);
	}
	dsp_comm->dsp_to_mcu.state = CPU_COMM_CMD_NULL;

	// wait dsp clear irq
	while(dsp_comm->mcu_to_dsp.state != CPU_COMM_CMD_NULL)
	{
		if(--count == 0)
		{
			ACT_LOG_ID_INF(ALF_STR_respond_dsp__DSP_OFFLINE_M2D_S0XX, 1,dsp_comm->mcu_to_dsp.state );
			return -ETIMEDOUT;
		}
	}
	return 0;
}

void dsp_comm_irq_handle(void)
{
//	printk("%s %d:\n", __func__, __LINE__);

	/*如果没有dsp的消息，但却有dsp的中断，直接返回，打印异常，并进入死循环*/
	while(dsp_comm->dsp_to_mcu.state != CPU_COMM_CMD_EXIST)
	{
	//print_buffer(0x9fc20140, 4, 16, 4, 0x9fc20140);
		//插入wait信号
//		dsp_do_wait();
//		ACT_LOG_ID_INF(ALF_STR_dsp_comm_irq_handle__UNKNOWN_COMM_IRQ_DSP, 1, dsp_comm->dsp_to_mcu.state);
        clear_dsp_irq_pending(IRQ_ID_OUT_USER2);
	    return;

	//print_buffer(0x9fc20140, 4, 16, 4, 0x9fc20140);

	}
	/*告知dsp，mcu正在处理它的消息*/
	dsp_comm->dsp_to_mcu.state = CPU_COMM_CMD_DEAL;

	clear_dsp_irq_pending(IRQ_ID_OUT_USER2);

	if(dsp_irq_callback)
		(*dsp_irq_callback)();
}


static int do_dsp_bank_miss_interrupt(unsigned int epc)
{
	unsigned char bk_group, type, bk_no;
	struct dsp *dsp;
	struct dsp_code dsp_code;
	uint32_t dsppageaddr;

	type = (uint8_t) ((epc & 0x1c000000) >> 26) & 0x03;
	bk_group = (uint8_t) ((epc & 0x03800000) >> 23) & 0x03;
	bk_no = (uint8_t) ((epc & 0x007c0000) >> 18);	//max is 0x1f

#if 0
	printk("%s: epc 0x%x, type %x, bk_group %x, bk_no %x\n",
		__func__, epc, type, bk_group, bk_no);
#endif

#if 0
	dsp = &dsps[type];
	sd_fseek(dsp->sd_file, 256 + (192 * bk_group) + (sizeof(dsp_code) * bk_no), SEEK_SET);
	if(sd_fread(dsp->sd_file, (void *)&dsp_code, sizeof(dsp_code)) != sizeof(dsp_code))
		return -EIO;
#else
	dsp = &dsps[type];
	memcpy((void *)&dsp_code,
		dsp->dsp_ptr + 256 + (192 * bk_group) + (sizeof(dsp_code) * bk_no),
		sizeof(dsp_code));
#endif

	if(dsp_code.length == 0)
		return -EIO;

	dsp_code.address = (void*)((unsigned int)dsp_code.address & 0x3ffff) + DSP_RAM_OFFSET;

	//printk("%s: dsp_code.address 0x%x, length 0x%x, offset 0x%x\n",
	//	__func__, dsp_code.address, dsp_code.length, dsp_code.offset);

#if 0
	sd_fseek(dsp->sd_file, dsp_code.offset, SEEK_SET);
	if(sd_fread(dsp->sd_file, dsp_code.address, dsp_code.length) != dsp_code.length)
		return -EIO;
#else
	memcpy(dsp_code.address,
		dsp->dsp_ptr + dsp_code.offset,
		dsp_code.length);

#endif
	epc = epc << 1;	//dsp is word unit

	dsppageaddr = sys_read32(DSP_PAGE_ADDR0);

	//check tlb is bit19~bit31
	if (sys_read32(DSP_PAGE_ADDR0 + (bk_group * 4)) == (epc & 0xfff80000))
		return -EBUSY;
	else
		sys_write32((epc & 0xfff80000), DSP_PAGE_ADDR0 + (bk_group << 2));

//	ACT_LOG_ID_INF(ALF_STR_do_dsp_bank_miss_interrupt__DSP_BANKXXN, 2, dsp_code.address, dsp_code.length);
//	if(type == 1)
 //   {
//	    pagemiss_type = 1;
 //   }
	return 0;
}

void dsp_bank_miss_irq_handle(void)
{
	unsigned int epc, dsp_bank;
	int ret = 0;

#if 0
	printk("%s %d:\n", __func__, __LINE__);
#endif

	/*如果没有dsp的消息，但却有dsp的中断，直接返回，打印异常，并进入死循环*/
	if(dsp_comm->dsp_to_mcu.state != CPU_COMM_CMD_EXIST) {
	    if(dsp_comm->dsp_to_mcu.state == CPU_COMM_CMD_NULL){
            clear_dsp_irq_pending(IRQ_ID_OUT_USER1);
            //ACT_LOG_ID_INF(ALF_STR_dsp_bank_miss_irq_handle__WARNING_DSP_STATEXN, 1, dsp_comm->dsp_to_mcu.state);
            return;
	    }else{
		    ACT_LOG_ID_INF(ALF_STR_dsp_bank_miss_irq_handle__UNKNOWN_BANK_MISS_IR, 1, dsp_comm->dsp_to_mcu.state);
		    return;
		}
	}

	/*告知dsp，mcu正在处理它的消息*/
	dsp_comm->dsp_to_mcu.state = CPU_COMM_CMD_DEAL;

#if 0
	printk("%s: content[1] 0x%x\n", __func__, dsp_comm->dsp_to_mcu.content[1]);
#endif

	/*如果有dsp的消息*/
	switch(dsp_comm->dsp_to_mcu.content[1])
	{
    	case CMD_REQ_OS_PAGEMISS:
    		epc = ((u8_t)dsp_comm->dsp_to_mcu.content[9] << 24) 
			| ((u8_t)dsp_comm->dsp_to_mcu.content[8] << 16) 
			| ((u8_t)dsp_comm->dsp_to_mcu.content[7] << 8)
			| ((u8_t)dsp_comm->dsp_to_mcu.content[6] ); 
    		
		    SYS_LOG_DBG("%s: epc 0x%x\n", __func__, epc);
		    
    	    ret = do_dsp_bank_miss_interrupt(epc);

			clear_dsp_irq_pending(IRQ_ID_OUT_USER1);

    		if(ret == 0)
    		{
    			ret = respond_dsp();
    		}

    		if(ret < 0)
    		{
    			//通知应用层, dsp bank miss处理失败了
    			if(dsp_irq_callback)
    				(*dsp_irq_callback)();

    			ACT_LOG_ID_INF(ALF_STR_dsp_bank_miss_irq_handle__DSP_BANK_MISS_IRQ_ER, 1, epc);
    			panic(0);
    			dsp_acts_stop(NULL);
    		}
    		//clear_dsp_irq_pending(irq);

    		return;

    	case CMD_REQ_OS_BANKPUSH:
    		dsp_bank = dsp_comm->dsp_to_mcu.content[4];
    		if(dsp_bank >= 22 && dsp_bank < 26)
    		{
    		   sys_write32(0, DSP_PAGE_ADDR0 + (dsp_bank * 4));
		}
    		break;

    	case CMD_REQ_OS_LOADDATA:
    	default:
    		break;
	}

    clear_dsp_irq_pending(IRQ_ID_OUT_USER1);
	respond_dsp();
	//clear_dsp_irq_pending(irq);
}

#if 0
#ifdef CONFIG_DSP_LIB_IN_SDFS
int init_dsp_lib(void)
{
	struct dsp_lib_info *dsp_lib;
	int i;

	for (i = 0; i < dsp_libs_cnt; i++) {
		dsp_lib = (struct dsp_lib_info *)&dsp_libs[i];

		if (sd_fmap(dsp_lib->name, (void **)&dsp_lib->ptr, &dsp_lib->size)) {
			SYS_LOG_ERR("cannot find dsp lib <%s> in SDFS", dsp_lib->name);
			return -EINVAL;
		}
	}

	return 0;
}
#endif

struct dsp_lib_info *match_dsp_lib(const char *file_name)
{
	struct dsp_lib_info *dsp_lib = NULL;
	int i;

	for (i = 0; i < dsp_libs_cnt; i++) {
		dsp_lib =(struct dsp_lib_info *) &dsp_libs[i];

		if (!strcmp(file_name, dsp_lib->name))
			return dsp_lib;
	}

	return NULL;
}
#else

uint32_t match_dsp_lib(const char *file_name, struct dsp_lib_info *dsp_lib)
{	
    if (sd_fmap(file_name, (void **)&dsp_lib->ptr, &dsp_lib->size)) {
        return -EINVAL;
    }

	return 0;
}

#endif

//static void inline set_dsp_vector_addr(unsigned int dsp_addr)
//{
//	sys_write32(mcu_to_dsp_code_address(dsp_addr), DSP_VCT_ADDR);
//}

//int dsp_acts_open(struct device *dev, char *file_name)

int dsp_acts_open(struct device *dev, const char *file_name, int type)
{
	struct dsp *dsp;
	struct dsp_info dsp_info;
	struct dsp_lib_info dsp_lib;
	unsigned char *dsp_code;
	unsigned int dsp_code_size;
	int i;
	u32_t addr, size, code_offs;

	dsp = &dsps[type];

	if (match_dsp_lib(file_name, &dsp_lib)) {
		SYS_LOG_ERR("cannot find dsp lib <%s>", file_name);
		return -EINVAL;
	}
	dsp_code = dsp_lib.ptr;
	dsp_code_size = dsp_lib.size;
	dsp->dsp_ptr = dsp_lib.ptr;

	memcpy((void *)&dsp_info, dsp_code, sizeof(dsp_info));
	if(memcmp(dsp_info.magic, "yqhx", 4) != 0) {
		SYS_LOG_ERR("dsp <%s> magic err\n", file_name);
		return -EINVAL;
	}

	code_offs = (u32_t)-1;
	//将dsp常驻代码及数据段拷贝到dsp ram中
	for(i = 0; i < sizeof(dsp_info.dsp_code)/sizeof(dsp_info.dsp_code[0]); i++)
	{
		addr = (u32_t)dsp_info.dsp_code[i].address;
		size = (u32_t)dsp_info.dsp_code[i].length;

		if (size == 0)
			continue;

		ACT_LOG_ID_INF(ALF_STR_dsp_acts_open____CODE_0X08X_0XXN, 2, addr, size);

		if (code_offs > dsp_info.dsp_code[i].offset)
			code_offs = dsp_info.dsp_code[i].offset;

		addr = (addr & 0x3ffff) + DSP_RAM_OFFSET;

		if (addr != (u32_t)DSP_IRQ_VECTOR_ADDR) {
		    /* DSP 内存映射分配
		     */
		//    dsp_mmap_alloc(type, addr, size);
		}

		memcpy((void*)addr,
			dsp_code + dsp_info.dsp_code[i].offset,
			size);
	}

	if(type == 0)
	{
		clear_dsp_pageaddr();
		//memset((void*)&dsp_comm->mcu_to_dsp, 0xee, sizeof(struct dsp_cmd) * 2);
	}

	SYS_LOG_INF("load dsp lib <%s>", file_name);

	return 0;
}

void dsp_acts_close(struct device *dev)
{
}

int dsp_acts_ready_start(struct device *dev, void *dsp_comm_irq_handler)
{
	ACT_LOG_ID_INF(ALF_STR_dsp_acts_ready_start__START_DSP, 0);

	/* assert dsp wait signal */
	dsp_do_wait();

	/* clear pd */
	clear_dsp_all_irq_pending();

	/*set dsp run start address*/
	set_dsp_vector_addr((unsigned int)DSP_ADDR);

	/* enable dsp clock*/
	acts_clock_peripheral_enable(CLOCK_ID_DSP);

	acts_clock_peripheral_enable(CLOCK_ID_DSP_COMM);

	/* reset dsp module */
	acts_reset_peripheral_assert(RESET_ID_DSP_ALL);

	dsp_irq_callback = dsp_comm_irq_handler;

	IRQ_CONNECT(IRQ_ID_OUT_USER1, CONFIG_IRQ_PRIO_HIGH,
		    dsp_bank_miss_irq_handle, NULL, 0);

	IRQ_CONNECT(IRQ_ID_OUT_USER2, CONFIG_IRQ_PRIO_HIGH,
		    dsp_comm_irq_handle, NULL, 0);

	/* deassert dsp wait signal */
	dsp_undo_wait();

	printk("%s %d:\n", __func__, __LINE__);

    return 0;
}

int dsp_acts_start(struct device *dev)
{
	acts_reset_peripheral_deassert(RESET_ID_DSP_ALL);

    /* enable dsp page miss irq */
    irq_enable(IRQ_ID_OUT_USER1);

    /* enable dsp communicate irq */
    irq_enable(IRQ_ID_OUT_USER2);

   return 0;
}
int dsp_acts_stop(struct device *dev)
{
	ACT_LOG_ID_INF(ALF_STR_dsp_acts_stop__STOP_DSP, 0);

	/* assert dsp wait signal */
	dsp_do_wait();

	/* disable dsp page miss irq */
	irq_disable(IRQ_ID_OUT_USER1);

	/* disable dsp communicate irq */
	irq_disable(IRQ_ID_OUT_USER2);

	dsp_irq_callback = NULL;

	/* assert reset dsp module */
	acts_reset_peripheral_assert(RESET_ID_DSP_ALL);

	/* disable dsp clock*/
	acts_clock_peripheral_disable(CLOCK_ID_DSP);

	acts_clock_peripheral_disable(CLOCK_ID_DSP_COMM);

	/* deassert dsp wait signal */
	dsp_undo_wait();

	clear_dsp_pageaddr();

	return 0;
}

#if 0
const struct dsp_driver_api dsp_acts_driver_api = {
	.open = dsp_acts_open,
	.close = dsp_acts_close,
	.read = dsp_acts_read,
};
#endif

int dsp_acts_init(struct device *dev)
{

	//printk("%s %d:\n", __func__, __LINE__);

#ifdef CONFIG_DSP_LIB_IN_SDFS
	//init_dsp_lib();
#endif

#if 0
	{
		volatile int ii = 1;
		while(ii == 1);
	}
	dsp_acts_open(dev, NULL);
	dsp_acts_ready_start(dev, NULL);
	dsp_acts_start(dev);
#endif
	return 0;
}

struct acts_dsp_data dsp_acts_ddata;

static const struct acts_dsp_config dsp_acts_cdata = {
//	.base = DSP_REG_BASE,
//	.clock_id = CLOCK_ID_DSP,
};

DEVICE_AND_API_INIT(dsp_acts, "dsp", dsp_acts_init,
		    &dsp_acts_ddata, &dsp_acts_cdata,
		    POST_KERNEL, 40,
		    NULL);
#if 0
static void dsp_acts_irq_config(struct device *dev)
{

	IRQ_CONNECT(IRQ_ID_OUT_USER2, 0,
		    dsp_comm_irq_handle, NULL, 0);
	irq_enable(IRQ_ID_OUT_USER2);
}
#endif
