#include <os_common_api.h>
#include <mem_manager.h>
#include <string.h>
#include <misc/printk.h>

#include "stub_hal.h"


struct stub_device* act_stub_open(char *dev_name, u8_t serial_num)
{
    struct stub_device *stub;

    stub = (struct stub_device *)mem_malloc(sizeof(struct stub_device));
	if (!stub)
		return NULL;

    memset((void *)stub, 0, sizeof(struct stub_device));

    stub->stub_dev = device_get_binding(dev_name);

    if (!stub->stub_dev)
    {
        printk("cannot found stub dev\n");
        mem_free(stub);
        return NULL;
    }

    //stub_open(stub->stub_dev, (void *)&serial_num);
    stub_open(stub->stub_dev);
    stub->open_flag = 1;

    return stub;
}

int stub_mod_cmd(struct stub_device* stub, void *param0, void *param1, void *param2, int cmd)
{
    if (stub->open_flag != 1)
    {
        printk("stub do not open yet!\n");
        return -1;
    }

    switch (cmd)
    {
    case STUB_OP_CLOSE:
        {
            printk("stub close!\n");
            stub_close(stub->stub_dev);
            mem_free(stub);

            return 0;
        }
    case STUB_OP_IOCTL:
        return stub_ioctl(stub->stub_dev, (u32_t)param0, param1, param2);

    case STUB_OP_READ:
        return stub_read(stub->stub_dev, (u32_t)param0, param1, (u32_t)param2);

    case STUB_OP_WRITE:
        return stub_write(stub->stub_dev, (u32_t)param0, param1, (u32_t)param2);

    case STUB_OP_EXT_RW:
        return stub_ext_rw(stub->stub_dev, (stub_ext_param_t *)param0, (u32_t)param1);

    case STUB_OP_RAW_RW:
        return stub_raw_rw(stub->stub_dev, (u8_t *)param0, (u32_t)param1, (u32_t)param2);
    }

    return -1;
}
