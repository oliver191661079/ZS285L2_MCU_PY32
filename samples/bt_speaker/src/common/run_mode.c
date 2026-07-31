#include "run_mode.h"
#include <property_manager.h>
#include <os_common_api.h>

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "run_mode"
#include <logging/sys_log.h>

static int run_mode = RUN_MODE_NORMAL;
static os_mutex run_mode_mutex;

void run_mode_init(void)
{
	run_mode = property_get_int(CFG_RUN_MODE, RUN_MODE_NORMAL);

    if (run_mode < RUN_MODE_MIN && run_mode >= RUN_MODE_MAX)
    {
        SYS_LOG_ERR("run_mode init error,init to normal\n");
        run_mode = RUN_MODE_NORMAL;
    }

    os_mutex_init(&run_mode_mutex);

    SYS_LOG_INF("run_mode:%d\n", run_mode);
}

bool run_mode_set(run_mode_e set_mode)
{
    int ret;

    if (set_mode < RUN_MODE_MIN && set_mode >= RUN_MODE_MAX)
    {
        SYS_LOG_ERR("run mode set param err\n");
        return false;
    }

    SYS_LOG_INF("%d\n", set_mode);

    os_mutex_lock(&run_mode_mutex, OS_FOREVER);

    ret = property_set_int(CFG_RUN_MODE, set_mode);
    if (ret != 0)
    {
        os_mutex_unlock(&run_mode_mutex);
        
        SYS_LOG_ERR("run mode property set err\n");
        return false;
    }

    run_mode = set_mode;

    os_mutex_unlock(&run_mode_mutex);

    return true;
}

int run_mode_get(void)
{
    int ret;

    os_mutex_lock(&run_mode_mutex, OS_FOREVER);

    ret = run_mode;

    os_mutex_unlock(&run_mode_mutex);

    return ret;
}

bool run_mode_is_normal(void)
{
    int ret;

    os_mutex_lock(&run_mode_mutex, OS_FOREVER);
    ret = run_mode;
    os_mutex_unlock(&run_mode_mutex);

    return (ret == RUN_MODE_NORMAL);
}

bool run_mode_is_demo(void)
{
    int ret;

    os_mutex_lock(&run_mode_mutex, OS_FOREVER);
    ret = run_mode;
    os_mutex_unlock(&run_mode_mutex);

    return (ret == RUN_MODE_DEMO);
}
