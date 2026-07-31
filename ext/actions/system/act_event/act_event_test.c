#include "act_event_inner.h"
#include <logging/sys_log.h>
#include <logging/log_core.h>

LOG_MODULE_REGISTER(act_event, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);

void event_test_compile(void)
{
    printk("log test:%d\n", __log_level);
    printk("check module %s level LOG_LEVEL_INF:%d\n", log_name_get(ACT_EVENT_ID_GET()),
            ACT_EVENT_CONST_LEVEL_CHECK(ACT_EVENT_LEVEL_INFO));
    SYS_LOG_INF("check test\n");
}

extern int cmd_print_act_event(void);


void act_event_test(void)
{
    //int i;

    os_sleep(100);

    //act_event_output_clear();
    event_test_compile();
    SYS_LOG_INF("event test start\n");

    SYS_EVENT_DBG(0, 1, 2, 3, 4);
	SYS_EVENT_INF(100);
	SYS_EVENT_INF(2, 2);
	SYS_EVENT_INF(3, 1, 2);
	SYS_EVENT_INF(4, 1, 2, 3);
	SYS_EVENT_INF(5, 1, 2, 3, 4);
	SYS_EVENT_INF(6, 1, 2, 3, 4, 5);
	SYS_EVENT_INF(7, "str");
    SYS_EVENT_INF(8, 1, 2, 3, 4);
    SYS_EVENT_WRN(9, 1, 2, 3, 4);
    SYS_EVENT_ERR(10, 1, 2, 3, 4);

    //for (i = 0; i < 1024; i++) {
    //    SYS_EVENT_INF(12, i);

    //    os_sleep(10);
    //}

	act_event_output_flush();

    printk("dump test\n");
    cmd_print_act_event();
    printk("dump test end\n");
}

