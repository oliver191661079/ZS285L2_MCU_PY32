/*
 * Copyright (c) 2026 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system_app_spp_uart_bridge.c
 *
 * @brief 蓝牙 SPP 服务（传统串口 UUID 0x1101）+ 可选 UART0 透传 / AVRCP ID3 输出。
 *
 * ============================================================================
 * 【你要测的路径】手机/PC APP --蓝牙 SPP--> 本机蓝牙模块 --解析--> 执行任务
 * ============================================================================
 *
 *   手机/PC 配对并连接 SPP（标准 UUID 00001101-...）
 *        |
 *        v
 *   协议栈 RFCOMM 收数据 -> bt_manager_spp.c -> bridge_spp_receive_data_cb()
 *        |
 *        +-- 模式 A【裸命令，推荐调试】：直接发 2 字节  D2 + 子命令
 *        |       例：下一曲 = D2 01
 *        |
 *        +-- 模式 B【带帧头】：A5 5A | LEN_H | LEN_L | payload...
 *        |       payload 为 D2 + 子命令（可多字节扩展）
 *        |
 *        v
 *   bt_manager_remote_cmd_dispatch(payload, len, "spp")
 *        |
 *        v
 *   bt_manager_media_* / volume_* / call_*  -> AVRCP/HFP -> 手机侧生效
 *
 * 日志关键字（串口调试口查看，前缀 [spp] / [remote_cmd]）：
 *   [spp] registered SPP 0x1101
 *   [spp] connected ch=
 *   [spp] RX len=
 *   [spp] bare cmd / framed cmd
 *   [remote_cmd] src=spp cmd D2 sub=
 *   [remote_cmd] src=spp xxx ret=0 (ok)
 *   [spp] TX ACK ...（仅 CONFIG_SYSTEM_APP_SPP_CMD_ACK=y）
 *
 * 回包格式（可选，prj.conf 开启 SPP_CMD_ACK）：
 *   裸 ACK 4 字节：D2 80 | 原子命令 | 状态（0=成功，非0 一般为 -errno）
 *   帧 ACK：A5 5A 00 04 D2 80 | 原子命令 | 状态
 *
 * 子命令表（payload[0]=0xD2, payload[1]=）：
 *   0x00 上一曲  0x01 下一曲  0x02 暂停  0x03 播放  0x04 播放/暂停
 *   0x05 音量+   0x06 音量-   0x07 音效   0x08 接听  0x09 挂断  0x0A 进 ADFU
 *
 * 测试说明见：samples/bt_speaker/tools/SPP_CMD_TEST.md
 *
 * --------------------------------------------------------------------------
 * 其它功能（与本需求无关时可忽略）
 * --------------------------------------------------------------------------
 * - CONFIG_SYSTEM_APP_SPP_UART_FORWARD=1：把非 D2 的 A5 5A 载荷转发到 UART0（旧桥接）
 * - AVRCP ID3：歌词/元数据经 bt_manager_avrcp_id3_uart_tx_byte() 输出到 UART0
 *
 * 初始化：system_bt_event_callback(BT_READY) -> system_app_spp_uart_bridge_init()
 */

#include <string.h>
#include <zephyr.h>
#include <device.h>
#include <uart.h>
#include <misc/printk.h>

#include "system_app.h"

#if defined(CONFIG_BT_SPP) || defined(CONFIG_BT_AVRCP_ID3_UART_DUMP)
#include <bt_manager.h>
#endif

/* 1=将 A5 5A 帧内非命令载荷转发 UART0；0=仅执行 SPP 命令（推荐） */
#ifndef CONFIG_SYSTEM_APP_SPP_UART_FORWARD
#define CONFIG_SYSTEM_APP_SPP_UART_FORWARD 0
#endif

/* -------------------------------------------------------------------------- */
/* UART0 TX：ID3 元数据 / 可选 SPP 透传                                          */
/* -------------------------------------------------------------------------- */

static struct device *uart0_tx_dev;

static void uart0_tx_ensure_bound(void)
{
	if (!uart0_tx_dev) {
		uart0_tx_dev = device_get_binding(CONFIG_UART_ACTS_PORT_0_NAME);
		if (!uart0_tx_dev) {
			SYS_LOG_ERR("uart0_tx: bind %s failed\n", CONFIG_UART_ACTS_PORT_0_NAME);
		}
	}
}

void system_app_uart0_tx_byte(uint8_t c)
{
	uart0_tx_ensure_bound();
	if (uart0_tx_dev) {
		uart_poll_out(uart0_tx_dev, c);
	}
}

#if CONFIG_SYSTEM_APP_SPP_UART_FORWARD
static void system_app_uart0_tx_buf(const uint8_t *data, size_t len)
{
	size_t i;

	if (!data || !len) {
		return;
	}
	for (i = 0; i < len; i++) {
		system_app_uart0_tx_byte(data[i]);
	}
}
#endif

#if defined(CONFIG_BT_AVRCP_ID3_UART_DUMP)
void bt_manager_avrcp_id3_uart_tx_byte(unsigned char c)
{
	system_app_uart0_tx_byte((uint8_t)c);
}
#endif

#ifdef CONFIG_BT_SPP

#define SPP_SYNC0			0xA5
#define SPP_SYNC1			0x5A
#define SPP_FRAME_HDR			4U
#define SPP_MAX_PAYLOAD			512U
#define SPP_ACC_CAP			(SPP_FRAME_HDR + SPP_MAX_PAYLOAD + 64U)

/* 回包子类型：D2 0x80（与命令 D2 0x00~0x09 区分） */
#define SPP_ACK_MARKER			0x80U
#define SPP_ACK_PAYLOAD_LEN		4U

/* 标准 SPP 串口 UUID：00001101-0000-1000-8000-00805F9B34FB */
static const uint8_t spp_service_uuid[16] = {
	0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
	0x00, 0x10, 0x00, 0x00, 0x01, 0x11, 0x00, 0x00,
};

static uint8_t rx_acc[SPP_ACC_CAP];
static size_t rx_acc_len;
static uint8_t spp_registered;
static uint8_t spp_channel;

#if defined(CONFIG_SYSTEM_APP_SPP_CMD_ACK)
/**
 * 将 bt_manager API 返回值编码为 ACK 状态字节。
 * 0=成功；失败时为 (uint8_t)(-ret)，ret>0 时用 0xFF。
 */
static uint8_t spp_ack_status_from_ret(int ret)
{
	if (ret == 0) {
		return 0U;
	}
	if (ret < 0) {
		return (uint8_t)(-ret);
	}
	return 0xFFU;
}

/**
 * 经 SPP 回 ACK（需 CONFIG_SYSTEM_APP_SPP_CMD_ACK=y 且已连接）。
 * @param subcmd  原命令子码（0x00~0x09）
 * @param ret     remote_cmd_dispatch 返回值
 * @param framed  1=用 A5 5A 帧封装（与入站帧一致）；0=裸 4 字节
 */
static void spp_send_cmd_ack(uint8_t subcmd, int ret, int framed)
{
	uint8_t ack[SPP_ACK_PAYLOAD_LEN];
	uint8_t frame[SPP_FRAME_HDR + SPP_ACK_PAYLOAD_LEN];
	int send_ret;

	if (!spp_channel) {
		printk("[spp] ACK skip: not connected sub=0x%02x\n",
		       (unsigned int)subcmd);
		return;
	}

	ack[0] = 0xD2u;
	ack[1] = SPP_ACK_MARKER;
	ack[2] = subcmd;
	ack[3] = spp_ack_status_from_ret(ret);

	if (framed) {
		frame[0] = SPP_SYNC0;
		frame[1] = SPP_SYNC1;
		frame[2] = 0U;
		frame[3] = (uint8_t)SPP_ACK_PAYLOAD_LEN;
		memcpy(frame + SPP_FRAME_HDR, ack, SPP_ACK_PAYLOAD_LEN);
		send_ret = bt_manager_spp_send_data(spp_channel, frame,
						    SPP_FRAME_HDR + SPP_ACK_PAYLOAD_LEN);
		printk("[spp] TX ACK framed sub=0x%02x st=%u cmd_ret=%d send_ret=%d\n",
		       (unsigned int)subcmd, (unsigned int)ack[3], ret, send_ret);
	} else {
		send_ret = bt_manager_spp_send_data(spp_channel, ack, SPP_ACK_PAYLOAD_LEN);
		printk("[spp] TX ACK bare D2 80 %02x %02x cmd_ret=%d send_ret=%d\n",
		       (unsigned int)ack[2], (unsigned int)ack[3], ret, send_ret);
	}
}
#endif /* CONFIG_SYSTEM_APP_SPP_CMD_ACK */

/**
 * 执行远程命令，可选 SPP 回包。
 * @param framed 入站是否为 A5 5A 帧（决定 ACK 是否带帧头）
 */
static void spp_dispatch_remote_cmd(const uint8_t *payload, uint16_t plen, int framed)
{
	int ret;

	ret = bt_manager_remote_cmd_dispatch(payload, plen, "spp");

#if defined(CONFIG_SYSTEM_APP_SPP_CMD_ACK)
	if (payload && plen >= 2U && payload[0] == 0xD2u) {
		spp_send_cmd_ack(payload[1], ret, framed);
	}
#endif
}

#if CONFIG_SYSTEM_APP_SPP_UART_FORWARD
static void uart0_emit_payload_line(const uint8_t *data, size_t len)
{
	system_app_uart0_tx_buf(data, len);
	system_app_uart0_tx_byte((uint8_t)'\r');
	system_app_uart0_tx_byte((uint8_t)'\n');
}
#endif

/**
 * 处理一帧 A5 5A 载荷：若以 D2 开头则执行蓝牙任务，否则可选转发 UART。
 */
static void spp_handle_frame_payload(const uint8_t *payload, size_t plen)
{
	if (!payload || plen < 2U) {
		printk("[spp] framed payload too short len=%u\n", (unsigned int)plen);
		return;
	}

	if (payload[0] == 0xD2u) {
		printk("[spp] framed cmd plen=%u sub=0x%02x\n",
		       (unsigned int)plen, (unsigned int)payload[1]);
		spp_dispatch_remote_cmd(payload, (uint16_t)plen, 1);
		return;
	}

#if CONFIG_SYSTEM_APP_SPP_UART_FORWARD
	printk("[spp] forward payload len=%u to UART0\n", (unsigned int)plen);
	uart0_emit_payload_line(payload, plen);
#else
	printk("[spp] framed payload not D2, ignored (enable SPP_UART_FORWARD to mirror)\n");
#endif
}

static void spp_process_accumulator(void)
{
	size_t i = 0;

	while (i + SPP_FRAME_HDR <= rx_acc_len) {
		uint16_t plen;
		size_t frame_total;

		if (rx_acc[i] != SPP_SYNC0 || rx_acc[i + 1] != SPP_SYNC1) {
			i++;
			continue;
		}

		plen = (uint16_t)(((uint16_t)rx_acc[i + 2] << 8) | rx_acc[i + 3]);
		if (plen > SPP_MAX_PAYLOAD) {
			SYS_LOG_WRN("spp: bad len %u, skip sync\n", (unsigned int)plen);
			i += 2;
			continue;
		}

		frame_total = SPP_FRAME_HDR + (size_t)plen;
		if (i + frame_total > rx_acc_len) {
			break;
		}

		spp_handle_frame_payload(rx_acc + i + SPP_FRAME_HDR, (size_t)plen);
		i += frame_total;
	}

	if (i > 0U && i < rx_acc_len) {
		memmove(rx_acc, rx_acc + i, rx_acc_len - i);
		rx_acc_len -= i;
	} else if (i >= rx_acc_len) {
		rx_acc_len = 0;
	} else if (rx_acc_len >= SPP_ACC_CAP) {
		SYS_LOG_WRN("spp: acc overflow, drop %u bytes\n", (unsigned int)rx_acc_len);
		rx_acc_len = 0;
	}
}

static void spp_feed_accumulator(const uint8_t *data, uint32_t len)
{
	if (!data || !len) {
		return;
	}
	if (rx_acc_len + len > sizeof(rx_acc)) {
		SYS_LOG_WRN("spp: chunk too large (%u), flush\n", (unsigned int)len);
		rx_acc_len = 0;
	}
	memcpy(rx_acc + rx_acc_len, data, len);
	rx_acc_len += len;
	spp_process_accumulator();
}

/**
 * 判断本包是否为「裸 D2 命令」（常见：手机 SPP 工具直接发 2 字节）。
 * 条件：以 D2 开头，长度 2~16，且不像 A5 5A 帧头。
 */
static int spp_try_bare_cmd(const uint8_t *data, uint32_t len)
{
	if (!data || len < 2U || len > 16U) {
		return 0;
	}
	if (data[0] != 0xD2u) {
		return 0;
	}
	/* 避免把 A5 5A 误当命令：若第二字节是 0x5A 且更长包，走帧解析 */
	if (len >= 4U && data[0] == 0xD2u && data[1] == 0x5Au) {
		return 0;
	}

	printk("[spp] bare cmd len=%u sub=0x%02x\n", (unsigned int)len,
	       (unsigned int)data[1]);
	spp_dispatch_remote_cmd(data, (uint16_t)len, 0);
	return 1;
}

static void bridge_spp_connect_failed_cb(uint8_t channel)
{
	printk("[spp] connect failed ch=%u\n", (unsigned int)channel);
	if (spp_channel == channel) {
		spp_channel = 0;
	}
}

static void bridge_spp_connected_cb(uint8_t channel, uint8_t *uuid)
{
	ARG_UNUSED(uuid);
	printk("[spp] connected ch=%u (ready for D2 commands)\n", (unsigned int)channel);
	spp_channel = channel;
}

static void bridge_spp_disconnected_cb(uint8_t channel)
{
	printk("[spp] disconnected ch=%u\n", (unsigned int)channel);
	if (spp_channel == channel) {
		spp_channel = 0;
	}
	rx_acc_len = 0;
}

/** SPP 数据接收入口（协议栈 -> 应用） */
static void bridge_spp_receive_data_cb(uint8_t channel, uint8_t *data, uint32_t len)
{
	ARG_UNUSED(channel);

	printk("[spp] RX ch=%u len=%u\n", (unsigned int)channel, (unsigned int)len);

	if (spp_try_bare_cmd(data, len)) {
		return;
	}

	/* 可能是 A5 5A 帧或分片，进入拼包状态机 */
	spp_feed_accumulator(data, len);
}

static const struct btmgr_spp_cb bridge_spp_cb = {
	.connect_failed = bridge_spp_connect_failed_cb,
	.connected = bridge_spp_connected_cb,
	.disconnected = bridge_spp_disconnected_cb,
	.receive_data = bridge_spp_receive_data_cb,
};

/**
 * @brief 蓝牙就绪后注册 SPP 0x1101，接收手机/PC 命令。
 */
void system_app_spp_uart_bridge_init(void)
{
	int ret;

	if (spp_registered) {
		return;
	}

	uart0_tx_ensure_bound();

	ret = bt_manager_spp_reg_uuid((uint8_t *)spp_service_uuid,
				      (struct btmgr_spp_cb *)&bridge_spp_cb);
	if (ret) {
		SYS_LOG_ERR("spp: reg uuid failed %d\n", ret);
		printk("[spp] init FAIL reg_uuid ret=%d\n", ret);
		return;
	}

	spp_registered = 1;
	printk("[spp] init OK: SPP 0x1101 cmd=D2+sub forward=%d ack=%d\n",
	       CONFIG_SYSTEM_APP_SPP_UART_FORWARD,
#if defined(CONFIG_SYSTEM_APP_SPP_CMD_ACK)
	       1);
#else
	       0);
#endif
	SYS_LOG_INF("spp: 0x1101 dispatch on ack=%d (SPP_CMD_TEST.md)\n",
#if defined(CONFIG_SYSTEM_APP_SPP_CMD_ACK)
		    1);
#else
		    0);
#endif
}

#endif /* CONFIG_BT_SPP */
