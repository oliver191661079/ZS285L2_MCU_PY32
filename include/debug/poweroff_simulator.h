#ifndef __POWERDOWN_SIMULATOR_H
#define __POWERDOWN_SIMULATOR_H

typedef enum{
	POWEROFF_SIM_MODULE_OTA 		= 0x10,
	POWEROFF_SIM_MODULE_NVRAM 		= 0x11,
	POWEROFF_SIM_MODULE_PROPERTY	= 0x12,
}poweroff_sim_module_e;

typedef enum{
	OTA_CHECK_STATE,
	OTA_CHECK_PREPARE_ERASE_INIT,
	OTA_CHECK_PREPARE_ERASE,
	OTA_CHECK_PREPARE_ERASE_CHECK,
	OTA_CHECK_WRITE_FILE_ERASE,
	OTA_CHECK_PARAM_UPDATE_ERASE,
	OTA_CHECK_FORCE_UPDATE_AGAIN,
}test_ota_id_e;

#ifdef CONFIG_POWEROFF_SIMULATOR

struct test_point;

typedef enum{
	TEST_POINT_MODE_NULL,
	TEST_POINT_MODE_REBOOT,
	TEST_POINT_MODE_DELAY_REBOOT,
}test_point_work_mode_e;

typedef struct {
	const uint16_t uid;
	//if return is 0, trigger actions else no trigger
	int (*const handler)(struct test_point *tp, void *param);
	const char *const desc;
} __packed test_point_info_t;

typedef struct {
	uint8_t mode;
	uint16_t delay_ms;
	uint32_t inject_value;
} test_point_ctx_t;

typedef struct test_point {
	sys_snode_t node;
	const test_point_info_t *info;
	test_point_ctx_t ctx;
} test_point_t;

void _trigger_test_point(uint16_t uid, void *param);
void _register_test_point(const test_point_info_t *info);

int poweroff_sim_init(void);

#define TEST_UID(_module, _id) ((((_module) & 0xFF) << 8) | ((_id) & 0xFF))
#define UID_TO_STR(m, id) "."_STRINGIFY(m)	"." _STRINGIFY(id)

#define REGISTER_TEST_POINT(_module_, _id, _handler) \
	static const test_point_info_t __tp_##_module_##_id \
	__attribute__((used, section(".test_points" UID_TO_STR(_module_, _id)))) = { \
		.uid = TEST_UID(_module_, _id), \
		.handler = _handler, \
		.desc = STRINGIFY(_id) \
	}


#define TRIGGER_TEST_POINT(_module_, _id, param) _trigger_test_point(TEST_UID(_module_, _id), param)

#else

#define REGISTER_TEST_POINT(_module_, _desc_, _id, _handler)

#define TRIGGER_TEST_POINT(_module_, _id, param)

#endif

#endif
