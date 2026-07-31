/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2017年11月9日-下午1:36:45             1.0             build this file
 ********************************************************************************/
/*!
 * \file     stub.c
 * \brief    stub协议定义
 * \author
 * \version  1.0
 * \date  2017年11月9日-下午1:36:45
 *******************************************************************************/

/*
 * stub_interface.c
 */
#include <string.h>
#include <stub/stub.h>
#include <stdlib.h>
#include <stub_hal.h>
#include "usb_stub.h"

//#define SYS_LOG_DOMAIN "STUB"
//#define SYS_LOG_LEVEL SYS_LOG_STUB_USB_LEVEL
#include <logging/sys_log.h>

//struct stub_config
//{
//    char *stub_name;
//    u16_t stub_type;
//};

struct stub_usb_data_t
{
	struct device *phy_dev;
    u32_t stub_usb_timeout;
};

static int stub_usb_write(struct device *dev, u16_t opcode, u8_t *data_buffer, u32_t data_len)
{
    struct stub_usb_data_t *stub = dev->driver_data;
    stub_trans_cmd_t cmd;
    int ret_val = 0;

    cmd.type = 0xae;
    cmd.opcode = (u8_t)(opcode >> 8);
    cmd.sub_opcode = (u8_t)opcode;
    cmd.reserv = 0;
    cmd.data_len = data_len;       //通常为0

    if (data_buffer != NULL){
        memcpy(data_buffer, &cmd, sizeof(cmd));

        ret_val = usb_stub_ep_write((u8_t *)data_buffer, (data_len + STUB_COMMAND_HEAD_SIZE), stub->stub_usb_timeout);
    } else{

        ret_val = usb_stub_ep_write((u8_t *)&cmd, (data_len + STUB_COMMAND_HEAD_SIZE), stub->stub_usb_timeout);
    }

    return ret_val;
}

static int stub_usb_read(struct device *dev, u16_t opcode, u8_t *data_buffer, u32_t data_len)
{
    struct stub_usb_data_t *stub = dev->driver_data;
    stub_trans_cmd_t cmd;
    int ret_val = 0;

    cmd.type = 0xae;
    cmd.opcode = (u8_t)(opcode >> 8);
    cmd.sub_opcode = (u8_t)opcode;
    cmd.reserv = 0;
    cmd.data_len = data_len;        //通常为0

    ret_val = usb_stub_ep_write((u8_t *)&cmd, (u32_t)sizeof(cmd), stub->stub_usb_timeout);
//  ret_val = phy_stub_write(stub->phy_dev, (u8_t *)&cmd, (u32_t)sizeof(cmd));

    if (ret_val != 0)
    {
        return -1;
    }

    if (cmd.opcode == 0xff)
    {
        data_len = 1;
    }

    // SYS_LOG_INF("%d", data_len);
    ret_val = usb_stub_ep_read(data_buffer, data_len, stub->stub_usb_timeout);
//  ret_val = phy_stub_read(stub->phy_dev, data_buffer, data_len);

    return ret_val;
}

static int stub_usb_ext_rw(struct device *dev, stub_ext_param_t *ext_param, u32_t rw_mode)
{
    struct stub_usb_data_t *stub = dev->driver_data;
    int i, cmd_len;
    u16_t *pdata;
    stub_ext_cmd_t *stub_ext_cmd;
    u16_t check_sum;
    int ret_val;

    stub_ext_cmd = (stub_ext_cmd_t *) ext_param->rw_buffer;

    pdata = (u16_t *) stub_ext_cmd;

    cmd_len = sizeof(stub_ext_cmd_t) + ext_param->payload_len;

    if (rw_mode == 1)
    {
        stub_ext_cmd->type = 0xAE;
        stub_ext_cmd->reserved = 0;
        stub_ext_cmd->opcode = (u8_t)(ext_param->opcode >> 8);
        stub_ext_cmd->sub_opcode = (u8_t)(ext_param->opcode);
        stub_ext_cmd->payload_len = ext_param->payload_len;

        check_sum = 0;
        for (i = 0; i < (cmd_len / 2); i++)
        {
            check_sum += pdata[i];
        }

        pdata[cmd_len / 2] = check_sum;

        //发送命令
        ret_val = usb_stub_ep_write((u8_t *)stub_ext_cmd,(u32_t)cmd_len + 2, stub->stub_usb_timeout);

        if (ret_val != 0)
        {
            ACT_LOG_ID_INF(ALF_STR_stub_usb_ext_rw__STUB_EXT_WRITE_ERROR, 0);
        }
    }
    else
    {
        ret_val = usb_stub_ep_read((u8_t *)stub_ext_cmd, (u32_t)cmd_len + 2, stub->stub_usb_timeout);

        if(ret_val == 0)
        {
            check_sum = 0;
            for (i = 0; i < (cmd_len / 2); i++)
            {
                check_sum += pdata[i];
            }

            if (pdata[cmd_len / 2] != check_sum)
            {
                ACT_LOG_ID_INF(ALF_STR_stub_usb_ext_rw__STUB_EXT_READ_ERRORN, 0);
            }
        }
    }

    return ret_val;
}

int stub_usb_ioctl(struct device *dev, u16_t op_code, void *param1, void *param2)
{
//  const struct stub_config *cfg = dev->config->config_info;

    return -1;
}

static int stub_usb_raw_rw(struct device *dev, u8_t *buf, u32_t data_len, u32_t rw_mode)
{
	return -1;
}

static int stub_usb_close(struct device *dev)
{
//  const struct stub_config *cfg = dev->config->config_info;

    SYS_LOG_INF("dev=%p", dev);

//  phy_stub_close(dev);
    return usb_stub_exit();
}

//extern int usb_phy_init(void);
static int stub_usb_open(struct device *dev)
{
    //const struct stub_config *cfg = dev->config->config_info;
    SYS_LOG_INF("dev=%p", dev);
    //phy_stub_open(dev);
    //usb_phy_init();
    return usb_stub_init(dev);
}

static const struct stub_driver_api stub_driver_api =
{
    .open = stub_usb_open,
    .close = stub_usb_close,
    .write = stub_usb_write,
    .read = stub_usb_read,
    .raw_rw = stub_usb_raw_rw,
    .ext_rw = stub_usb_ext_rw,
    .ioctl = stub_usb_ioctl,
};

static int stub_usb_init(struct device *dev)
{
//  const struct stub_config *cfg = dev->config->config_info;
    struct stub_usb_data_t *stub = dev->driver_data;

//  stub->phy_dev = device_get_binding(cfg->stub_name);
//
//  if (!stub->phy_dev)
//  {
//      SYS_LOG_ERR("cannot found stub phy dev %s\n", cfg->stub_name);
//      return -ENODEV;
//  }

//    dev->driver_api = &stub_driver_api;

    stub->stub_usb_timeout = 500;

    return 0;
}

//const struct stub_config stub_usb_cfg =
//{
//    .stub_name = CONFIG_STUB_USB_NAME,
//    .stub_type = STUB_PHY_USB,
//};

static struct stub_usb_data_t stub_usb_data;

DEVICE_AND_API_INIT(usb_stub, CONFIG_STUB_DEV_USB_NAME,
            stub_usb_init, &stub_usb_data, NULL,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &stub_driver_api);

