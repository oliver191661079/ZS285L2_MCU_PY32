/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
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

 * Author: wh<wanghui@actions-semi.com>
 *
 * Change log:
 *	2017/1/20: Created by wh.
 */


#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <shell/shell.h>
#include <stdlib.h>
#include <limits.h>

#define EXT_NET_SHELL "extnet"

extern void test_iperf_server(int argc, char *argv[]);
extern void test_iperf_client(int argc, char *argv[]);
extern int Cli_RunCmd_arg(int argc, char *argv[]);

static int shell_cmd_wifi(int argc, char *argv[])
{
	Cli_RunCmd_arg((argc -1), &argv[1]);

	return 0;
}

static int shell_iperf_server(int argc, char *argv[])
{
	test_iperf_server((argc -1), &argv[1]);
	return 0;
}

static int shell_iperf_client(int argc, char *argv[])
{
	test_iperf_client((argc -1), &argv[1]);
	return 0;
}

static struct shell_cmd ext_net_commands[] = {
#if defined(CONFIG_WIFI)
	{ "wifi", shell_cmd_wifi, "Wifi command operate interface"},
#endif
	{ "iperfs", shell_iperf_server, "Start iperf server"},
	{ "iperfc", shell_iperf_client, "Start iperf client"},
	{ NULL, NULL, NULL }
};

SHELL_REGISTER(EXT_NET_SHELL, ext_net_commands);
