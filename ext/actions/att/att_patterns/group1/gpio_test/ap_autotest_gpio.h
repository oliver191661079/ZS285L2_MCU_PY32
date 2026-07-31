#ifndef _AP_AUTOTEST_GPIO_H
#define _AP_AUTOTEST_GPIO_H

#define GPIO_REG_BASE			0xc0090000

#define GPIO_REGA_BASE         (0xc0090000)

#define GPIO_REGB_BASE         (0xc0090014)

#define GPIO_REGSIO_BASE       (0xc0090028)

typedef struct
{
    uint32 out_en;
    uint32 in_en;
    uint32 dat;
    //pull-up
    uint32 pu_en;
    //pull-down
    uint32 pd_en;
}gpio_ret_t;

typedef enum
{
    GPIO_TEST_OK = 0,

}gpio_test_result_e;

#endif

