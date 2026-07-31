#ifndef __STUB_H__
#define __STUB_H__

#include <zephyr/types.h>
#include <device.h>

#define STUB_COMMAND_HEAD_SIZE  8

#define OT_10MS_132MHZ  1200
#define OT_500MS_72MHZ  300000
#define OT_500MS_132MHZ 600000

#define STUB_DELAY_100MS    (100)
#define STUB_DELAY_200MS    (100)
#define STUB_DELAY_500MS    (500)
#define STUB_DELAY_1000MS   (1000)

#define STUB_TRANS_EXT_CMD_HEADER_LEN  (8)

//trans command
typedef struct
{
    u8_t type;
    u8_t opcode;
    u8_t sub_opcode;
    u8_t reserv;
    u32_t data_len;
} stub_trans_cmd_t;

typedef struct
{
    u8_t type;
    u8_t opcode;
    u8_t sub_opcode;
    u8_t reserved;
    u16_t payload_len;
}stub_ext_cmd_t;

typedef enum
{
    STUB_USE_URAM0 = 0,
    STUB_USE_URAM1
} stub_uram_type_e;

typedef enum
{
    STUB_PHY_USB,
    STUB_PHY_UART
} stub_phy_interface_e;

typedef enum
{
    SET_TIMEOUT = 0,
    RESET_FIFO = 1,
    SWITCH_URAM = 2,
    SET_STATUS = 3,
    GET_CONNECT_STATE = 4,
    SET_LED_STATE = 5,
    SET_READ_MODE = 6,
    SET_DEBUG_MODE = 7,
    GET_PHY_INTERFACE = 8,
    CLR_RECV_DATA = 9,
} stub_ioctl_t;

typedef struct
{
    u16_t opcode;
    u16_t payload_len;
    u8_t *rw_buffer;
} stub_ext_param_t;

typedef enum
{
    /** open stub */
    STUB_OP_OPEN = 0,
    /** close stub */
    STUB_OP_CLOSE = 1,
    /** set param*/
    STUB_OP_IOCTL = 2,
    /** read */
    STUB_OP_READ = 3,
    /** write */
    STUB_OP_WRITE = 4,
    /** read write ext */
    STUB_OP_EXT_RW = 5,
    /*! read write raw */
    STUB_OP_RAW_RW
} stub_cmd_t;

//pc tools type
typedef enum
{
    STUB_PC_TOOL_INVALID = 0,
    STUB_PC_TOOL_ASQT_MODE = 1,
    STUB_PC_TOOL_ASET_MODE = 2,
    STUB_PC_TOOL_ASET_EQ_MODE = 3,
    STUB_PC_TOOL_ATT_MODE = 4,
    STUB_PC_TOOL_TK_PMODE = 5,
    STUB_PC_TOOL_ECTT_MODE = 6,
    STUB_PC_TOOL_WAVES_ASET_MODE = 7,
    STUB_PC_TOOL_SERIAL_MONITOR = 8,
    STUB_PC_TOOL_RETT_MODE = 9,
    STUB_PC_TOOL_DUMP_MODE = 0x0d,
    STUB_PC_TOOL_BTT_MODE = 0x42,
    STUB_PC_TOOL_COMMON_DAE_MODE = 0x80,
} PC_stub_mode_e;


struct stub_driver_api {
    int (*open)(struct device *dev);
	int (*close)(struct device *dev);
	int (*write)(struct device *dev, u16_t opcode, u8_t *data_buffer, u32_t data_len);
	int (*read)(struct device *dev, u16_t opcode, u8_t *data_buffer, u32_t data_len);
    int (*raw_rw)(struct device *dev, u8_t *buf, u32_t data_len, u32_t rw_mode);
    int (*ext_rw)(struct device *dev, stub_ext_param_t *ext_param, u32_t rw_mode);
    int (*ioctl)(struct device *dev, u16_t op_code, void *param1, void *param2);
};

static inline int stub_open(struct device *dev)
{
    const struct stub_driver_api *api = dev->driver_api;

    return api->open(dev);
}

static inline int stub_close(struct device *dev)
{
    const struct stub_driver_api *api = dev->driver_api;

    return api->close(dev);
}

static inline int stub_read(struct device *dev, u16_t opcode, u8_t *data_buffer, u32_t data_len)
{
    const struct stub_driver_api *api = dev->driver_api;

    return api->read(dev, opcode, data_buffer, data_len);
}

static inline int stub_write(struct device *dev, u16_t opcode, u8_t *data_buffer, u32_t data_len)
{
    const struct stub_driver_api *api = dev->driver_api;

    return api->write(dev, opcode, data_buffer, data_len);
}

static inline int stub_raw_rw(struct device *dev, u8_t *buf, u32_t data_len, u32_t rw_mode)
{
    const struct stub_driver_api *api = dev->driver_api;

    return api->raw_rw(dev, buf, data_len, rw_mode);
}

static inline int stub_ext_rw(struct device *dev, stub_ext_param_t *ext_param, u32_t rw_mode)
{
    const struct stub_driver_api *api = dev->driver_api;

    return api->ext_rw(dev, ext_param, rw_mode);
}

static inline int stub_ioctl(struct device *dev, u16_t op_code, void *param1, void *param2)
{
    const struct stub_driver_api *api = dev->driver_api;

    return api->ioctl(dev, op_code, param1, param2);
}

static inline int stub_set_debug_mode(struct device *dev, int param1)
{
    const struct stub_driver_api *api = dev->driver_api;

    return api->ioctl(dev, (u16_t)SET_DEBUG_MODE, (void *)param1, (void *)0);
}

#endif

