
/*
 * uart_stub.c
 */
#include <stub/stub.h>
#include <uart_stub.h>

//struct stub_config
//{
//    char *stub_name;
//    u16_t stub_type;
//};

struct stub_uart_data_t
{
	struct device *phy_dev;
    u32_t stub_uart_timeout;
    s8_t stub_uart_connect_state;
    u8_t stub_uart_read_mode;
    u8_t stub_uart_debug_mode;
    u8_t stub_uart_rx_gpio;
    uart_session_request_t stub_uart_request;
};

static int stub_uart_check_line(struct stub_uart_data_t *stub)
{
    uart_session_request_t *q = &stub->stub_uart_request;

    if (stub->stub_uart_rx_gpio != 0)
        return 0;

    q->request = STUB_OP_IOCTL;
    q->timeout = 10;
    q->debug_mode = stub->stub_uart_debug_mode;

    q->param[0] = GET_CONNECT_STATE;
    q->param[1] = 0;
    q->param[2] = 0;

    q->send_data = NULL;
    q->send_size = 0;

    q->recv_data = NULL;
    q->recv_size = 0;

//    UART_RX_GPIO = 21;  // 先尝试 A21 进行连接
//
//    if (uart_session_request(q) == 0)
//        return 0;

    stub->stub_uart_rx_gpio = 2;  // 再尝试 A16 进行连接

    if (uart_session_request(q) == 0)
        return 0;

//    UART_RX_GPIO = 21;  // 连接失败时切换至 A21

    return -1;
}

static int stub_uart_ioctl(struct device *dev, u16_t opcode, void *param1, void *param2)
{
    struct stub_uart_data_t *stub = dev->driver_data;
    uart_session_request_t *q = &stub->stub_uart_request;

    int result;

    switch (opcode)
    {
    case SET_TIMEOUT:
        stub->stub_uart_timeout = (s32_t)param1;
        break;

    case SET_READ_MODE:
        stub->stub_uart_read_mode = (u8_t)((u32_t)param1);
        return 0;

    case SET_DEBUG_MODE:
        stub->stub_uart_debug_mode = (u8_t)((u32_t)param1);
        return 0;

    case GET_CONNECT_STATE:
        if (param1 == 0)
            return stub->stub_uart_connect_state;
        break;
    }

    stub_uart_check_line(stub);

    q->request = STUB_OP_IOCTL;
    q->timeout = stub->stub_uart_timeout;
    q->debug_mode = stub->stub_uart_debug_mode;

    q->param[0] = opcode;
    q->param[1] = (u16_t)((u32_t)param1);
    q->param[2] = (u16_t)((u32_t)param2);

    q->send_data = NULL;
    q->send_size = 0;

    q->recv_data = NULL;
    q->recv_size = 0;

    result = uart_session_request(q);

    if (opcode == GET_CONNECT_STATE)
    {
        /* 已连接时返回 UART_RX1: A21 或 A16 */
        if (result >= 0)
            result = stub->stub_uart_rx_gpio;

        stub->stub_uart_connect_state = (s8_t)result;
    }

    return result;
}

static int stub_uart_open(struct device *dev)
{
    int ret;

    ret = stub_uart_check_line(dev->driver_data);

    if (ret < 0)
    {
        return ret;
    }
    /* 重置读取模式 */
    stub_uart_ioctl(dev, SET_READ_MODE, 0, 0);

    /* 重置服务端超时 */
    ret = stub_uart_ioctl(dev, SET_TIMEOUT, (void *)STUB_DELAY_500MS, 0);
    if (ret < 0)
    {
        return ret;
    }

    /* 重置服务端 URAM */
    ret = stub_uart_ioctl(dev, SWITCH_URAM, (void *)STUB_USE_URAM1, 0);
    if (ret < 0)
    {
        return ret;
    }

    /* 重置服务端 IN FIFO */
    ret = stub_uart_ioctl(dev, RESET_FIFO, 0, 0);
    if (ret < 0)
    {
        return ret;
    }

    /* 重置服务端 OUT FIFO */
    ret = stub_uart_ioctl(dev, RESET_FIFO, (void *)1, 0);
    if (ret < 0)
    {
        return ret;
    }
    return 0;
}


static int stub_uart_close(struct device *dev)
{
    return 0;
}


static int stub_uart_request_try(uart_session_request_t *q, u8_t stub_uart_read_mode)
{
    int result;
    u32_t start_time = k_uptime_get_32();
    try:
    result = uart_session_request(q);
    if (result != 0 && stub_uart_read_mode != 0
        && k_uptime_get_32() - start_time < 10000)
    {
        k_sleep(100);
        goto try;
    }

    return result;
}

static int stub_uart_read(struct device *dev, u16_t opcode, u8_t *buf, u32_t len)
{
    struct stub_uart_data_t *stub = dev->driver_data;
    uart_session_request_t *q = &stub->stub_uart_request;

    int result;

    q->request = STUB_OP_READ;
    q->timeout = stub->stub_uart_timeout;
    q->debug_mode = stub->stub_uart_debug_mode;

    q->param[0] = opcode;
    q->param[1] = len;
    q->param[2] = 0;

    q->send_data = NULL;
    q->send_size = 0;

    q->recv_data = buf;
    q->recv_size = len;

    result = stub_uart_request_try(q, stub->stub_uart_read_mode);

    return result;
}

static int stub_uart_write(struct device *dev, u16_t opcode, u8_t *buf, u32_t len)
{
    struct stub_uart_data_t *stub = dev->driver_data;
    uart_session_request_t *q = &stub->stub_uart_request;

    int result;

    q->request = STUB_OP_WRITE;
    q->timeout = stub->stub_uart_timeout;
    q->debug_mode = stub->stub_uart_debug_mode;

    q->param[0] = opcode;
    q->param[1] = len;
    q->param[2] = 0;

    if (buf == NULL || len == 0)
    {
        q->send_data = NULL;
        q->send_size = 0;
    }
    else  // buf 前面是 8 字节的命令头空间, 后面是 len 字节的数据.
    {
        /* 命令头 */
        {
            stub_trans_cmd_t *cmd = (stub_trans_cmd_t *)buf;

            cmd->type = 0xAE;
            cmd->opcode = (u8_t)(opcode >> 8);
            cmd->sub_opcode = (u8_t)(opcode);
            cmd->reserv = 0;
            cmd->data_len = len;
        }

        q->send_data = buf;
        q->send_size = STUB_COMMAND_HEAD_SIZE + len;
    }

    q->recv_data = NULL;
    q->recv_size = 0;

    result = uart_session_request(q);

    return result;
}


static int stub_uart_ext_rw(struct device *dev, stub_ext_param_t *ext_param, u32_t rw_mode)
{
    struct stub_uart_data_t *stub = dev->driver_data;
    uart_session_request_t *q = &stub->stub_uart_request;

    int result;

    q->request = STUB_OP_EXT_RW;
    q->timeout = stub->stub_uart_timeout;
    q->debug_mode = stub->stub_uart_debug_mode;

    q->param[0] = ext_param->opcode;
    q->param[1] = ext_param->payload_len;
    q->param[2] = rw_mode;

    /* rw_buffer
       前面是 6 字节的命令头空间,
       中间是 payload_len 字节的数据,
       末尾是 2 字节的校验和. */

    if (rw_mode == 1)  // 发送数据
    {
        /* 命令头 */
        {
            stub_ext_cmd_t *cmd = (stub_ext_cmd_t *)ext_param->rw_buffer;

            cmd->type = 0xAE;
            cmd->opcode = (u8_t)(ext_param->opcode >> 8);
            cmd->sub_opcode = (u8_t)(ext_param->opcode);
            cmd->reserved = 0;
            cmd->payload_len = ext_param->payload_len;
        }

        /* 校验和 */
        {
            u16_t checksum = 0;

            u16_t *buf = (u16_t *)ext_param->rw_buffer;
            int i, n;

            n = (sizeof(stub_ext_cmd_t) + ext_param->payload_len) / 2;

            for (i = 0; i < n; i++) checksum += buf[i];

            buf[n] = checksum;
        }

        q->send_data = ext_param->rw_buffer;
        q->send_size = sizeof(stub_ext_cmd_t) + ext_param->payload_len + 2;

        q->recv_data = NULL;
        q->recv_size = 0;
    }
    else  // 读取数据
    {
        q->send_data = NULL;
        q->send_size = 0;

        q->recv_data = ext_param->rw_buffer;
        q->recv_size = sizeof(stub_ext_cmd_t) + ext_param->payload_len + 2;
    }

    result = stub_uart_request_try(q, stub->stub_uart_read_mode);

    return result;
}

static int stub_uart_raw_rw(struct device *dev, u8_t *buf, u32_t len, u32_t rw_mode)
{
    struct stub_uart_data_t *stub = dev->driver_data;
    uart_session_request_t *q = &stub->stub_uart_request;

    int result;

    q->request = STUB_OP_RAW_RW;
    q->timeout = stub->stub_uart_timeout;
    q->debug_mode = stub->stub_uart_debug_mode;

    q->param[0] = len;
    q->param[1] = rw_mode;
    q->param[2] = 0;

    if (rw_mode == 1)  // 发送数据
    {
        q->send_data = buf;
        q->send_size = len;

        q->recv_data = NULL;
        q->recv_size = 0;
    }
    else  // 读取数据
    {
        q->send_data = NULL;
        q->send_size = 0;

        q->recv_data = buf;
        q->recv_size = len;
    }

    result = uart_session_request(q);

    return result;
}

static const struct stub_driver_api stub_driver_api =
{
    .open = stub_uart_open,
    .close = stub_uart_close,
    .write = stub_uart_write,
    .read = stub_uart_read,
    .raw_rw = stub_uart_raw_rw,
    .ext_rw = stub_uart_ext_rw,
    .ioctl = stub_uart_ioctl,
};

static int uart_stub_init(struct device *dev)
{
//  const struct stub_config *cfg = dev->config->config_info;
    struct stub_uart_data_t *stub = dev->driver_data;

//  stub->phy_dev = device_get_binding(cfg->stub_name);
//
//  if (!stub->phy_dev)
//  {
//      SYS_LOG_ERR("cannot found stub phy dev %s\n", cfg->stub_name);
//      return -ENODEV;
//  }

//  dev->driver_api = &stub_driver_api;

    stub->stub_uart_connect_state = -1;
    stub->stub_uart_debug_mode = 0;
    stub->stub_uart_read_mode = 0;
    stub->stub_uart_rx_gpio = 0;
    stub->stub_uart_timeout = STUB_DELAY_500MS;

    return 0;
}


static struct stub_uart_data_t stub_uart_data;

//const struct stub_config stub_uart_cfg =
//{
//    .stub_name = CONFIG_STUB_UART_NAME,
//    .stub_type = STUB_PHY_UART,
//};

DEVICE_AND_API_INIT(uart_stub, CONFIG_STUB_DEV_UART_NAME,
            uart_stub_init, &stub_uart_data, NULL,
            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &stub_driver_api);

