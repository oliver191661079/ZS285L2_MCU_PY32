
/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief UART Driver for Actions SoC
 */


#include <kernel.h>
#include <device.h>
#include <uart.h>
#include <soc.h>
#include <board.h>
#include <init.h>
#include <uart_stub.h>
#include <uart.h>
#include <trace.h>
#include <drivers/console/uart_console.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_UART_STUB

extern void console_input_init(struct device *dev);
extern void uart_console_hook_install(void);

static struct device *uart_stub_dev;


static int uart_ctrl_write(u8_t *buf, int len, u32_t flags)
{
    u32_t t, start_cycles;
    int i;
    struct acts_uart_controller *uart = UART_STRUCT(uart_stub_dev);
    struct acts_uart_data *uart_data = DEV_DATA(uart_stub_dev);

    if (flags & UART_TX_START)
    ;

    if (flags & UART_TX_END)
    {
        t = 160 * 1000000 / uart_data->baud_rate;  // 16 字节大小的 FIFO
        t *= 2;

        if (t < 2)
            t = 2;
    }

    for (i = 0; i < len; i++)
    {
        /* TX FIFO 为满状态? */
        if (uart->stat & UART_STA_TFFU)
        {
            /* 等待 2 个字节的传输时间 */
            k_busy_wait(20 * 1000000 / uart_data->baud_rate);

            /* TX FIFO 为满状态? */
            if (uart->stat & UART_STA_TFFU)  // 传输错误?
            {
                i = -1;
                break;
            }
        }
        /* 写入数据 */
        uart->txdat = (u32_t)buf[i];
    }

    if (flags & UART_TX_END)
    {
        start_cycles = k_cycle_get_32();
        /* 等待 TX FIFO 所有数据传输完成 */
        while (uart->stat & UART_STA_UTBB)
        {
            if (usec_spent_time(start_cycles) > t)  // 传输错误?
            {
                i = -1;
                break;
            }
        }
    }

    return i;
}

static int uart_ctrl_read(u8_t *buf, int len, u32_t timeout)
{
    int i;
    struct acts_uart_controller *uart = UART_STRUCT(uart_stub_dev);
    struct acts_uart_data *uart_data = DEV_DATA(uart_stub_dev);

    if (timeout > 0)
    {
        u32_t start_cycles = k_cycle_get_32();

        /* 等待 RX FIFO 不为空状态 */
        while (uart->stat & UART_STA_RFEM)
        {
            if (usec_spent_time(start_cycles) > timeout * 1000)
                break;
        }
    }

    for (i = 0; i < len; i++)
    {
        // uart_poll_in(uart_stub_dev, buf + i);
        //RX FIFO 为空状态?
        if (uart->stat & UART_STA_RFEM)
        {
            //等待 2 个字节的传输时间
            k_busy_wait(20 * 1000000 / uart_data->baud_rate);

            // RX FIFO 为空状态?
            if (uart->stat & UART_STA_RFEM)
            {
                break;
            }
        }

        //读取数据
        buf[i] = (unsigned char)uart->rxdat;

    }

    return i;
}


static int uart_data_print(char *str, void *data, int size)
{
    u8_t *p = (u8_t *)data;
    int i;

    if (str != NULL)
    {
        for (i = 0; str[i] != '\0'; i++)
        ;

        uart_ctrl_write((u8_t *)str, i, 0);
    }

    if (size > 128)
        return 0;

    for (i = 0; i < size; i++)
    {
        u8_t ch[2];

        ch[0] = (p[i] >> 4);
        ch[1] = (p[i] & 0x0f);

        if (ch[0] >= 10)
            ch[0] += 'A' - 10;
        else
            ch[0] += '0';

        if (ch[1] >= 10)
            ch[1] += 'A' - 10;
        else
            ch[1] += '0';

        if (i > 0 && (i % 4) == 0)
            uart_ctrl_write(" ", 1, 0);

        uart_ctrl_write(ch, 2, 0);
    }

    return i;
}


static u16_t uart_data_checksum(u8_t *data, int size)
{
    u16_t checksum = 0;
    int i;

    for (i = 0; i < size; i++)
    {
        checksum += data[i];
    }

    return checksum;
}

static void uart_ctrl_clear(void)
{
    struct acts_uart_controller *uart = UART_STRUCT(uart_stub_dev);

    /* 复位 FIFO */
    uart->stat |= UART_STA_ERRS | UART_STA_RIP | UART_STA_RIP;
}

static int uart_session_send(u8_t type, u8_t *data, int size)
{
    u32_t irq_flag;
    int retry_times;

    u8_t buf[8];
    u16_t checksum;

    /* 重试次数 */
    retry_times = 0;

    /* 校验和 */
    checksum = uart_data_checksum(data, size);

start:
    irq_flag = irq_lock();

    /* 读空接收的未知数据 */
    while (uart_ctrl_read(buf, sizeof(buf), 0) > 0)
    ;

    /* 发送数据信息 */
    buf[0] = UART_PACKAGE;
    buf[1] = (u8_t)(type);
    buf[2] = (u8_t)(size >> 8);
    buf[3] = (u8_t)(size);


    if (uart_ctrl_write(buf, 4, UART_TX_START | UART_TX_END) != 4)
    {
        goto retry;
    }
    /* 清空 FIFO 准备接收 */
    uart_ctrl_clear();

    /* 读取回应 */
    if (uart_ctrl_read(buf, 2, UART_RSP_TIMEOUT) < 1)
    {
        goto retry;
    }

    /* 错误回应? */
    if (buf[0] != UART_RSP_YES &&
        buf[1] != UART_RSP_YES)
    {
        goto retry;
    }

    /* 发送数据 */
    if (uart_ctrl_write(data, size, UART_TX_START) != size)
        goto retry;

    /* 发送校验和 */
    if (uart_ctrl_write((u8_t *)&checksum, 2, UART_TX_END) != 2)
        goto retry;

    /* 清空 FIFO 准备接收 */
    uart_ctrl_clear();

    /* 读取回应 */
    if (uart_ctrl_read(buf, 2, UART_RSP_TIMEOUT) < 1)
        goto retry;


    /* 错误回应? */
    if (buf[0] != UART_RSP_YES &&
        buf[1] != UART_RSP_YES)
        goto retry;

    irq_unlock(irq_flag);

    return size;

retry:
    uart_ctrl_clear();

    irq_unlock(irq_flag);

    if (++retry_times < 3)
        goto start;

    return -1;
}

static int uart_session_recv(u8_t type, u8_t *data, int size, u32_t timeout)
{
    int irq_flag;
    int retry_times;

    u8_t buf[8];
    u16_t checksum;

    /* 重试次数 */
    retry_times = 0;

start:
    /* 读取数据包标志 */
    if (uart_ctrl_read(&buf[0], 1, timeout) != 1)
        return -1;

    irq_flag = irq_lock();

    if (buf[0] != UART_PACKAGE)
        goto retry;

    /* 读取数据信息 */
    if (uart_ctrl_read(&buf[1], 3, 0) != 3)
        goto retry;

    if (buf[1] != (u8_t)(type) ||
        buf[2] != (u8_t)(size >> 8) ||
        buf[3] != (u8_t)(size))
        goto retry;

    /* 正确回应 */
    buf[0] = UART_RSP_YES;
    buf[1] = UART_RSP_YES;

    uart_ctrl_write(buf, 2, UART_TX_START | UART_TX_END);

    /* 清空 FIFO 准备接收 */
    uart_ctrl_clear();

    /* 读取数据 */
    if (uart_ctrl_read(data, size, UART_RSP_TIMEOUT) != size)
        goto retry;

    /* 读取校验和 */
    if (uart_ctrl_read((u8_t *)&checksum, 2, 0) != 2)
        goto retry;

    if (checksum != uart_data_checksum(data, size))
        goto retry;

    /* 正确回应 */
    buf[0] = UART_RSP_YES;
    buf[1] = UART_RSP_YES;

    uart_ctrl_write(buf, 2, UART_TX_START | UART_TX_END);

    /* 清空 FIFO 准备接收 */
    uart_ctrl_clear();

    irq_unlock(irq_flag);

    return size;

retry:
    /* 接收请求命令时使用轮循方式而不用重试? */
    if (type == UART_REQUEST)
    {
        irq_unlock(irq_flag);
        return -1;
    }

    /* 读空接收的错误数据 */
    while (uart_ctrl_read(buf, sizeof(buf), 0) > 0)
    ;

    /* 错误回应 */
    buf[0] = UART_RSP_NO;
    buf[1] = UART_RSP_NO;

    uart_ctrl_write(buf, 2, UART_TX_START | UART_TX_END);

    /* 清空 FIFO 准备接收 */
    uart_ctrl_clear();

    irq_unlock(irq_flag);

    if (++retry_times < 3)
    {
        timeout = UART_RSP_TIMEOUT;
        goto start;
    }

    return -1;
}

static int uart_ctrl_init(uart_baud_rate_mode mode)
{
    u8_t c;

    switch (mode)
    {
    case UART_BR_ATT_MODE:
        uart_baud_rate_switch(uart_stub_dev, UART_BR_ATT_MODE);
        uart_irq_rx_disable(uart_stub_dev);
        uart_irq_tx_disable(uart_stub_dev);

        while (uart_irq_rx_ready(uart_stub_dev))
        {
            uart_fifo_read(uart_stub_dev, &c, 1);
        }
        break;

    case UART_BR_CPU_MODE:
        uart_baud_rate_switch(uart_stub_dev, UART_BR_NORMAL_MODE);
        uart_irq_rx_disable(uart_stub_dev);
        uart_irq_tx_disable(uart_stub_dev);

        while (uart_irq_rx_ready(uart_stub_dev))
        {
            uart_fifo_read(uart_stub_dev, &c, 1);
        }
        break;

    case UART_BR_PRINTK_CPU:
        uart_baud_rate_switch(uart_stub_dev, UART_BR_NORMAL_MODE);
        console_input_init(uart_stub_dev);
        uart_console_hook_install();
        break;
        
    case UART_BR_NORMAL_MODE:
    default:
        break;
    }
    return 0;
}

int uart_session_request(uart_session_request_t *q)
{
    char result;
    char err;
    int i, n;

    u32_t timeout = q->timeout;
    u32_t debug_mode = q->debug_mode;

    if (timeout != (u32_t) - 1)
        timeout += 1000;

    k_sched_lock();

    uart_ctrl_init(UART_BR_ATT_MODE);

    result = 0;
    err = 0;

    if (q->send_size > UART_STUB_BUF_SIZE ||
        q->recv_size > UART_STUB_BUF_SIZE)
    {
        err = 1;
        goto end;
    }

    /* 发送请求命令 */
    if (uart_session_send(UART_REQUEST, (u8_t *)q, 12) < 0)
    {
        err = 2;
        goto end;
    }

    /* 分块发送数据 */
    for (i = 0; i < q->send_size; i += n)
    {
        n = q->send_size - i;

        if (n > UART_PACKAGE_SIZE)
            n = UART_PACKAGE_SIZE;

        if ((n = uart_session_send(UART_DATA_SEND, &q->send_data[i], n)) < 0)
        {
            err = 3;
            goto end;
        }
    }

    /* 接收服务端命令处理结果 */
    if (uart_session_recv(UART_SERVICE, (u8_t *)&result, 1, timeout) < 0)
    {
        err = 4;
        goto end;
    }

    /* 错误结果? */
    if (result == (char)-1)
    {
        err = 5;
        goto end;
    }

    /* 分块接收数据 */
    for (i = 0; i < q->recv_size; i += n)
    {
        n = q->recv_size - i;

        if (n > UART_PACKAGE_SIZE)
            n = UART_PACKAGE_SIZE;

        if ((n = uart_session_recv(UART_DATA_RECV, &q->recv_data[i], n, timeout)) < 0)
        {
            err = 6;
            goto end;
        }
    }

    /* 校验和错误? */
    if (result == (char)-2)
    {
        err = 7;
        goto end;
    }

    /* 结束 */
end:
    if (result == 0 && err != 0)
        result = -1;

    if (debug_mode || err != 0)
    {
        uart_ctrl_init(UART_BR_CPU_MODE);

        uart_ctrl_write("  \r\nQ:", 6, UART_TX_START);

        uart_data_print(" ", &q->request, 1);
        uart_data_print(" ", &q->param[0], 2);
        uart_data_print(" ", &q->send_size, 2);
        uart_data_print(" ", &q->recv_size, 2);
        uart_data_print(" ", &err, 1);

        if (q->send_size > 0)
            uart_data_print(" ", q->send_data, q->send_size);

        if (q->recv_size > 0 && (err == 0 || err == 7))
            uart_data_print(" ", q->recv_data, q->recv_size);

        uart_ctrl_write("\r\n", 2, UART_TX_END);

    }
    uart_ctrl_init(UART_BR_PRINTK_CPU);

    k_sched_unlock();

    return result;
}

int att_stub_init(struct device *dev)
{
    ARG_UNUSED(dev);
    uart_stub_dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
    if (uart_stub_dev == NULL)
    {
        ACT_LOG_ID_INF(ALF_STR_att_stub_init__GET_BIND_UART_DEVICE, 0);
    }

    return 0;
}

SYS_INIT(att_stub_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

