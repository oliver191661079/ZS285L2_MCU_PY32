#include "btcall.h"
#include <shell/shell.h>

#define BT_CALL_SHELL_MODULE "call"
#define BT_CALL_RING_TEST_NUM 4
#define BT_CALL_MAX_PPHONE_NUM 20

typedef struct {
	btcall_ring_handle_t handle;
	char numbers[BT_CALL_MAX_PPHONE_NUM + 1];
} btcall_ring_test_t;

static btcall_ring_test_t ring_handles[BT_CALL_RING_TEST_NUM] = {NULL};

static int call_test_ring_start(int argc, char *argv[])
{
	u8_t i;
	u8_t len;

	if (argc != 2) {
		SYS_LOG_INF("Wrong paramters %d", argc);
		return -1;
	}

	len = strlen(argv[1]);
	if(len == 0 || len > BT_CALL_MAX_PPHONE_NUM) {
		return -2;
	}
	SYS_LOG_INF("%s", argv[1]);

	//find free ring handle
	for (i = 0; i < BT_CALL_RING_TEST_NUM; i++) {
		if (NULL == ring_handles[i].handle) {
			break;
		}
	}
	if (i < BT_CALL_RING_TEST_NUM) {
		strncpy(ring_handles[i].numbers, argv[1], len);
		ring_handles[i].handle = btcall_ring_mgr_start(argv[1]);
	} else {
		SYS_LOG_INF("max ring");
	}

	return 0;
}

static int call_test_ring_stop(int argc, char *argv[])
{
	u8_t i;
	u8_t len;

	if (argc != 2) {
		SYS_LOG_INF("Wrong paramters %d", argc);
		return -1;
	}

	len = strlen(argv[1]);
	if(len == 0 || len > BT_CALL_MAX_PPHONE_NUM) {
		return -2;
	}
	SYS_LOG_INF("%s", argv[1]);

	//find ring handle
	for (i = 0; i < BT_CALL_RING_TEST_NUM; i++) {
		if (ring_handles[i].handle
		    && !memcmp(ring_handles[i].numbers, argv[1], len)) {
			break;
		}
	}
	if (i < BT_CALL_RING_TEST_NUM) {
		btcall_ring_mgr_stop(ring_handles[i].handle);
		ring_handles[i].handle = NULL;
	} else {
		SYS_LOG_INF("no ring");
	}

	return 0;
}

static int call_test_ring_init(int argc, char *argv[])
{
	SYS_LOG_INF("");
	btcall_ring_mgr_init();
	return 0;
}

static int call_test_ring_deinit(int argc, char *argv[])
{
	SYS_LOG_INF("");
	btcall_ring_mgr_deinit();
	return 0;
}

static struct shell_cmd btcall_test_cmd[] = {
	{"ri", call_test_ring_init, "ring init"},
	{"rd", call_test_ring_deinit, "deinit ring "},
	{"rs", call_test_ring_start, "start play ring "},
	{"rt", call_test_ring_stop, "stop play ring "},
	{NULL, NULL, NULL}
};

SHELL_REGISTER(BT_CALL_SHELL_MODULE, btcall_test_cmd);
