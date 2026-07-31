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
#include <stdlib.h>
#include <string.h>
#include <misc/printk.h>
#include <net/net_ip.h>
#include <net/net_core.h>
#include <net/net_pkt.h>
#include <net/net_app.h>
#include <mem_manager.h>
#include <zephyr.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_IPERF_CLIENT_SHELL

#define TCPUDP_TIMEOUT	3000
#define DST_PORT  5001
#define SEND_PACKET_LEN	1400

static struct net_app_ctx tcpudp;

static void test_iperf_tcpudp_tx(struct net_app_ctx *ctx, int runtime, bool tcp)
{
	struct net_pkt *pkt = NULL;
	int i, ret;
	int txDataCnt = 0;
	int timecnt = 0;
	uint32_t preTime = 0;
	uint32_t curTime = 0;
	char num = '0';
	char *data;

	data = (char *)mem_malloc(SEND_PACKET_LEN);
	if (!data) {
		return;
	}

	curTime = k_uptime_get_32();
	preTime = curTime;

	while (timecnt < runtime) {
		curTime = k_uptime_get_32();
		if ((curTime - preTime) > 1000) {
			printk("iperf %s tx:%d kbps\n", (tcp? "tcp" : "udp"), ((txDataCnt*8)/1024));
			timecnt++;
			preTime = curTime;
			txDataCnt = 0;
		}

		pkt = net_app_get_net_pkt(ctx, AF_UNSPEC, TCPUDP_TIMEOUT);
		if (!pkt) {
			printk("%s,%d\n", __func__, __LINE__);
			continue;
		}

		for (i=0; i<SEND_PACKET_LEN; i++) {
			data[i] = num;
			num++;
			if (num > '9')  {
				num = '0';
			}
		}

		ret = net_pkt_append_all(pkt, SEND_PACKET_LEN, data, TCPUDP_TIMEOUT);
		if (ret != true) {
			printk("%s,%d\n", __func__, __LINE__);
			net_pkt_unref(pkt);
			continue;
		}

		ret = net_app_send_pkt(ctx, pkt, NULL, 0, K_FOREVER, NULL);
		if (ret < 0) {
			printk("%s,%d\n", __func__, __LINE__);
			net_pkt_unref(pkt);
			continue;
		}

		txDataCnt += SEND_PACKET_LEN;
	}

	mem_free(data);
}

static void tcpudp_received(struct net_app_ctx *ctx,
			 struct net_pkt *pkt,
			 int status,
			 void *user_data)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(status);
	ARG_UNUSED(user_data);

	if (!pkt) {
		return;
	}

	net_pkt_unref(pkt);
}

static int start_tcp_connect(struct net_app_ctx *ctx, const char *host)
{
	int ret;

	ret = net_app_init_tcp_client(ctx, NULL, NULL, host, DST_PORT,
					  TCPUDP_TIMEOUT, NULL);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_tcp_connect__CANNOT_INIT_TCP_CLIE, 1, ret);
		goto fail;
	}


	ret = net_app_set_cb(ctx, NULL, tcpudp_received, NULL, NULL);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_tcp_connect__CANNOT_SET_CALLBACKS, 1, ret);
		goto fail;
	}

	ret = net_app_connect(ctx, TCPUDP_TIMEOUT);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_tcp_connect__CANNOT_CONNECT_TCP_D, 1, ret);
		goto fail;
	}

fail:
	return ret;
}

static int start_udp_connect(struct net_app_ctx *ctx, const char *host)
{
	int ret;

	ret = net_app_init_udp_client(ctx, NULL, NULL, host, DST_PORT,
					  TCPUDP_TIMEOUT, NULL);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_udp_connect__CANNOT_INIT_UDP_CLIE, 1, ret);
		goto fail;
	}

	ret = net_app_set_cb(ctx, NULL, tcpudp_received, NULL, NULL);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_udp_connect__CANNOT_SET_CALLBACKS, 1, ret);
		goto fail;
	}

	ret = net_app_connect(ctx, TCPUDP_TIMEOUT);
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_start_udp_connect__CANNOT_CONNECT_UDP_D, 1, ret);
		goto fail;
	}

fail:
	return ret;
}

static void stop_tcpudp(struct net_app_ctx *ctx)
{
	net_app_close(ctx);
	net_app_release(ctx);
}
void test_iperf_client(int argc, char *argv[])
{
	int ret;
	int runtime;
	int prio;
	bool testtcp;

	if (argc != 3) {
		ACT_LOG_ID_INF(ALF_STR_test_iperf_client__USEDEXTNET_IPERFC_IP, 0);
		return;
	}

	prio = k_thread_priority_get(k_current_get());
	k_thread_priority_set(k_current_get(), 10);

	runtime = strtoul(argv[1], NULL, 10);
	if (!strcmp(argv[2], "tcp")) {
		testtcp = true;
		ret = start_tcp_connect(&tcpudp, argv[0]);
	} else {
		testtcp = false;
		ret = start_udp_connect(&tcpudp, argv[0]);
	}

	if (ret) {
		SYS_LOG_ERR("%s %s error:%d", __func__, argv[2], ret);
		goto exit;
	}

	test_iperf_tcpudp_tx(&tcpudp, runtime, testtcp);

exit:
	stop_tcpudp(&tcpudp);

	k_thread_priority_set(k_current_get(), prio);
}
