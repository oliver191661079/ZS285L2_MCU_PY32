#ifndef __RUN_MODE_H__
#define __RUN_MODE_H__

#include <stdbool.h>

typedef enum {
    RUN_MODE_MIN = 0,
    RUN_MODE_NORMAL = 0,
    RUN_MODE_DEMO,
    RUN_MODE_MAX,
} run_mode_e;

void run_mode_init(void);
bool run_mode_set(run_mode_e set_mode);
int run_mode_get(void);
bool run_mode_is_normal(void);
bool run_mode_is_demo(void);

#endif
