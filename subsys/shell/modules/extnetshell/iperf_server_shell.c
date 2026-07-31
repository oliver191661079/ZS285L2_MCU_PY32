/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <errno.h>
#include <misc/printk.h>
#include <net/net_ip.h>
#include <net/net_core.h>
#include <net/net_pkt.h>
#include <zephyr.h>
#include <net/net_app.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_IPERF_SERVER_SHELL


#define MY_PORT  5001
static struct net_app_ctx tcp;
static struct net_app_ctx udp;

static int rxDataCnt = 0;
static u32_t preTime = 0;
static u32_t curTime = 0;
static int udp_rxDataCnt = 0;
static u32_t udp_preTime = 0;
static u32_t udp_curTime = 0;

static void tcp_received(struct net_app_ctx *ctx,
			 struct net_pkt *pkt,
			 int status,
			 void *user_data)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(status);
	ARG_UNUSED(user_data);

	if (!pkt) {
		/* EOF condition */
		return;
	}

	rxDataCnt += net_pkt_appdatalen(pkt);
	curTime = k_uptime_get_32();

	if ((curTime - preTime) > 1000) {
		ACT_LOG_ID_INF(ALF_STR_tcp_received__IPERF_TCP_RXD_KBPSN, 1, ((rxDataCnt*8)/1024));
		rxDataCnt = 0;
		preTime = curTime;
	}

	net_pkt_unref(pkt);
}

static void start_tcp_iperf_server(void)
{
	int ret;

	ret = net_app_init_tcp_server(&tcp, NULL, MY_PORT, NULL);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_tcp_iperf_server__CANNOT_INIT_TCP_SERV, 1, MY_PORT);
		return;
	}

	ret = net_app_set_cb(&tcp, NULL, tcp_received, NULL, NULL);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_tcp_iperf_server__CANNOT_SET_CALLBACKS, 1, ret);
		net_app_release(&tcp);
		return;
	}

	net_app_server_enable(&tcp);

	ret = net_app_listen(&tcp);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_tcp_iperf_server__CANNOT_WAIT_CONNECTI, 1, ret);
		net_app_release(&tcp);
		return;
	}
}

static void udp_received(struct net_app_ctx *ctx,
			 struct net_pkt *pkt,
			 int status,
			 void *user_data)
{
	if (!pkt) {
		return;
	}

	udp_rxDataCnt += net_pkt_get_len(pkt);
	udp_curTime = k_uptime_get_32();

	if ((udp_curTime - udp_preTime) > 1000) {
		ACT_LOG_ID_INF(ALF_STR_udp_received__IPERF_UDP_RXD_KBPSN, 1, ((udp_rxDataCnt*8)/1024));
		udp_rxDataCnt = 0;
		udp_preTime = udp_curTime;
	}

	net_pkt_unref(pkt);
}

static void start_udp_iperf_server(void)
{
	int ret;

	ret = net_app_init_udp_server(&udp, NULL, MY_PORT, NULL);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_udp_iperf_server__CANNOT_INIT_UDP_SERV, 1, MY_PORT);
		return;
	}

	ret = net_app_set_cb(&udp, NULL, udp_received, NULL, NULL);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_udp_iperf_server__CANNOT_SET_CALLBACKS, 1, ret);
		net_app_release(&udp);
		return;
	}

	net_app_server_enable(&udp);

	ret = net_app_listen(&udp);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_udp_iperf_server__CANNOT_WAIT_CONNECTI, 1, ret);
		net_app_release(&udp);
		return;
	}
}

void test_iperf_server(int argc, char *argv[])
{
	static bool start = false;

	if ((argc == 1) && (!strcmp(argv[0], "stop"))) {
		if (!start) {
			ACT_LOG_ID_INF(ALF_STR_test_iperf_server__ERROR_IPERF_SERVER_A, 0);
			return;
		}

		net_app_server_disable(&tcp);
		net_app_close(&tcp);
		net_app_release(&tcp);

		net_app_close(&udp);
		net_app_release(&udp);
		start = false;
	} else {
		if (start) {
			ACT_LOG_ID_INF(ALF_STR_test_iperf_server__ERROR_IPERF_SERVER_A, 0);
			return;
		}

		start_tcp_iperf_server();
		start_udp_iperf_server();

		ACT_LOG_ID_INF(ALF_STR_test_iperf_server__START_IPERF_SERVER_S, 0);
		start = true;
	}
}
