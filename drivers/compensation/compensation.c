/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2019-2-18-上午10:42:00             1.0             build this file
 ********************************************************************************/
/*!
 * \file     compensation.c
 * \brief
 * \author
 * \version  1.0
 * \date  2019-2-18-上午10:42:00
 *******************************************************************************/
#include <init.h>
#include <kernel.h>
#include <string.h>
#include <compensation.h>
#include <property_manager.h>
#include <thread_timer.h>
#include <soc_atp.h>
#include <nvram_config.h>
#include <soc.h>

#define SYS_LOG_DOMAIN "comp"

#ifndef SYS_LOG_LEVEL
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_DEFAULT_LEVEL
#endif
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_COMPENSATION

//3*8=24bit
#define MAX_EFUSE_CAP_RECORD  (CONFIG_COMPENSATION_FREQ_INDEX_NUM)

typedef struct
{
    uint8_t value[MAX_EFUSE_CAP_RECORD];
} cap_value_t;

//#define HOSC_CALIB_SIM

//#define DEBUG_COMP_READ_WRITE

#ifdef HOSC_CALIB_SIM
static uint8_t hosc_val[MAX_EFUSE_CAP_RECORD];

static int soc_get_hosc_calib_sim(int id, unsigned char *calib_val)
{
    *calib_val = hosc_val[id];
    return 0;
}

static int soc_set_hosc_calib_sim(int id, unsigned char calib_val)
{
    hosc_val[id] = calib_val;
    return 0;
}
#endif

static uint32_t read_efuse_freq_value(uint32_t *read_cap_value, int *index)
{
    int i;
    cap_value_t cap_value;
	int retval = 0;

	memset(&cap_value, 0, sizeof(cap_value));

    for (i = 0; i < MAX_EFUSE_CAP_RECORD; i++) {
#ifndef HOSC_CALIB_SIM
        retval = soc_atp_get_hosc_calib(i, &cap_value.value[i]);
#else
        soc_get_hosc_calib_sim(i, &cap_value.value[i]);
#endif
		if(retval){
			SYS_LOG_ERR("write read efuse err");
			*read_cap_value = 0xff;
			*index = 0xff;
			return -EIO;
		}
    }

    //越靠后的efuse值越新，返回最后一次修改值
    for (i = (MAX_EFUSE_CAP_RECORD - 1); i >= 0; i--) {
        if (cap_value.value[i] != 0) {
            break;
        }
    }

    //未写过efuse
    if (i < 0) {
		*read_cap_value = 0xff;
		*index = 0xff;
    }else{
		*read_cap_value = cap_value.value[i];
		*index = i;
	}

    return 0;
}

static int32_t read_efuse_freq_compensation(uint32_t *cap_value, int *index)
{
    uint32_t trim_cap_value;
	int retval;

    retval = read_efuse_freq_value(&trim_cap_value, index);

	if(retval == 0){
		if (*index != 0xff) {
			*cap_value = trim_cap_value;
		}else{
			*cap_value = 0;
		}
	}

    return retval;
}

static int32_t spi_nor_freq_compensation_param_read(uint32_t *trim_cap)
{
    int32_t ret_val;

    ret_val = nvram_config_get(CFG_BT_CFO_VAL, (void *) trim_cap, sizeof(u8_t));

    if(ret_val > 0){
        return 0;
    }else{
        return -EIO;
    }
}

int32_t freq_compensation_read(uint32_t *trim_cap, uint32_t mode)
{
    int ret_val;
    int index;
    uint32_t trim_cap_value_bak;

    ret_val = read_efuse_freq_compensation(trim_cap, &index);

#ifdef DEBUG_COMP_READ_WRITE
    ACT_LOG_ID_INF(ALF_STR_freq_compensation_read__READ_COMP_D_INDEX_DN, 2, *trim_cap, index);
#endif

	if(ret_val){
		return TRIM_CAP_READ_ERR_HW;
	}

    if (index == 0xff) {
        //efuse未写频偏值，直接返回错误
        ret_val = TRIM_CAP_READ_ERR_NO_WRITE;
    } else if ((index < (MAX_EFUSE_CAP_RECORD - 1)) && (index > 0)) {
        //第2组有有效数据，则返回频偏值
        ret_val = TRIM_CAP_READ_NO_ERROR;
    } else if (index == 0){
        //第1组efuse值有效，需要特殊处理
        ret_val = TRIM_CAP_READ_ADJUST_VALUE;
    } else {
        //第3组有有效数据，还要判断flash上是否有数据
        ret_val = spi_nor_freq_compensation_param_read(&trim_cap_value_bak);

#ifdef DEBUG_COMP_READ_WRITE
        ACT_LOG_ID_INF(ALF_STR_freq_compensation_read__READ_ON_FLASH_D_DN, 2, trim_cap_value_bak, ret_val);
#endif

        if (ret_val != 0) {
            //norflash中不存在频偏值
            if (index == (MAX_EFUSE_CAP_RECORD - 1)) {
                //第3组efuse值有效
                ret_val = TRIM_CAP_READ_NO_ERROR;
            } else {
                ret_val = TRIM_CAP_ERAD_ERR_VALUE;
            }
        } else {
            if (mode == RW_TRIM_CAP_EFUSE) {
                //参数区数据有效，efuse区数据无效，返回无效数据标志
                ret_val = TRIM_CAP_ERAD_ERR_VALUE;
            } else {
                //更新为norflash中的参数
                *trim_cap = trim_cap_value_bak;

                //参数区数据有效
                ret_val = TRIM_CAP_READ_NO_ERROR;
            }
        }
    }

#ifdef DEBUG_COMP_READ_WRITE
    ACT_LOG_ID_INF(ALF_STR_freq_compensation_read__READ_OVER_DN, 1, ret_val);
#endif

    return ret_val;
}

static int32_t write_efuse_new_value(int new_value, int old_index, int new_index)
{
    int ret_val = 0;

    if(new_index == 1 || new_index == 2){
#ifndef HOSC_CALIB_SIM
        ret_val = soc_atp_set_hosc_calib(new_index, new_value);
#else
        ret_val = soc_set_hosc_calib_sim(new_index, new_value);
#endif
		if(ret_val){
			ret_val = -EIO;
		}
    }else{
		ret_val = -EIO;
	}

	ACT_LOG_ID_INF(ALF_STR_write_efuse_new_value__WRITING_TO_EFUSE_D_D, 3, new_index, new_value, ret_val);

    return ret_val;
}

static int32_t write_efuse_freq_compensation(uint32_t *cap_value)
{
    int trim_cap_value;
    int index;
    int old_cap_value;
	int retval;

    //只写低8bit
    trim_cap_value = (*cap_value) & 0xff;

	//读出旧的efuse值
    retval = read_efuse_freq_value(&old_cap_value, &index);

	if(retval){
		return retval;
	}

    ACT_LOG_ID_INF(ALF_STR_write_efuse_freq_compensation__READ_VAL_D_NEW_VAL_D, 3, old_cap_value, trim_cap_value, index);

    if (index != 0xff) {
        // 当和初始值相等时，就必须新添加一个频偏值
        if ((trim_cap_value > (old_cap_value - 3))&&(trim_cap_value < (old_cap_value + 3)) && (index != 0)) {
			//强制重新刷新写一次数值
            return write_efuse_new_value(old_cap_value, index, index);
        } else {
            // 新添加一个频偏值
            //efuse已写满
            if (index == (MAX_EFUSE_CAP_RECORD - 1)) {
				SYS_LOG_ERR("efuse full\n");
                return -ENOMEM;
            } else {
                return write_efuse_new_value(trim_cap_value, index, index + 1);
            }
        }
    } else {
        //从未烧写过efuse, 频偏从第二组，也就是index为1的组开始写
        //第一组默认是给FT测试写的
        return write_efuse_new_value(trim_cap_value, index, 1);
    }
}


static int32_t spi_nor_freq_compensation_param_write(uint32_t *trim_cap)
{
    int32_t ret_val;
    ACT_LOG_ID_INF(ALF_STR_spi_nor_freq_compensation_param_write__WRITING_TO_NORN, 0);
    ret_val = nvram_config_set(CFG_BT_CFO_VAL, (void *) trim_cap, sizeof(u8_t));

    return ret_val;
}

int32_t freq_compensation_write(uint32_t *trim_cap, uint32_t mode)
{
    int ret_val;

    //总共两字节,是对称的两字节数据
    *trim_cap &= 0xffff;

    ret_val = write_efuse_freq_compensation(trim_cap);

    if (mode == RW_TRIM_CAP_SNOR) {
        //efuse已写满
        if (ret_val == -ENOMEM) {

#ifdef DEBUG_COMP_READ_WRITE
            ACT_LOG_ID_INF(ALF_STR_freq_compensation_write__WRITE_ON_FLASHN, 0);
#endif

            //写norflash
            ret_val = spi_nor_freq_compensation_param_write(trim_cap);

            if (ret_val == 0) {
                ret_val = TRIM_CAP_WRITE_NO_ERROR;
            } else {
                ret_val = TRIM_CAP_WRITE_ERR_HW;
            }
        } else if (ret_val == -EIO) {
            ret_val = TRIM_CAP_WRITE_ERR_HW;
        } else {
            ret_val = TRIM_CAP_WRITE_NO_ERROR;
        }
    } else {
        //efuse已写满
        if (ret_val == -ENOMEM) {
            ret_val = TRIM_CAP_WRITE_ERR_NO_RESOURSE;
        } else if (ret_val == -EIO) {
            ret_val = TRIM_CAP_WRITE_ERR_HW;
        } else {
            ret_val = TRIM_CAP_WRITE_NO_ERROR;
        }
    }

#ifdef DEBUG_COMP_READ_WRITE
    ACT_LOG_ID_INF(ALF_STR_freq_compensation_write__WRITE_OVER_DN, 1, ret_val);
#endif

    return ret_val;
}




/* 设置蓝牙频偏
 */
static int freq_compensation_init(struct device *arg)
{
    uint32_t cap_adjust = 0;
    int ret_val;

#ifdef CONFIG_USE_EXT_CAP
	cap_adjust = 0;
	soc_set_hosc_cap(cap_adjust);
	return 0;
#endif

    ret_val = freq_compensation_read(&cap_adjust, RW_TRIM_CAP_SNOR);

    SYS_LOG_INF("cap adjust %x %d\n", cap_adjust, ret_val);

    if (ret_val == TRIM_CAP_READ_ADJUST_VALUE) {
        /* 校正处理流程
         */
        cap_adjust = CONFIG_COMPENSATION_DEFAULT_HOSC_CAP;
		soc_set_hosc_cap(cap_adjust);
    } else {
        if (ret_val != TRIM_CAP_READ_NO_ERROR) {
            cap_adjust = CONFIG_COMPENSATION_DEFAULT_HOSC_CAP;
        }
        soc_set_hosc_cap(cap_adjust);
    }

    return 0;
}

SYS_INIT(freq_compensation_init, POST_KERNEL, CONFIG_COMPENSATION_INIT_PRIORITY_DEVICE)
;

