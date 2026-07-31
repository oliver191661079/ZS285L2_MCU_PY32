#ifndef _STUB_HAL_H
#define _STUB_HAL_H

#include <msg_manager.h>
#include <drivers/stub/stub.h>

//#include <logging/sys_log.h>

struct stub_device
{
    struct device * stub_dev;
    u8_t open_flag;
};


extern int stub_mod_cmd(struct stub_device*, void *, void *, void *, int);


/*Start the stub and connect the device to the PC */
/*int stub_init(bool need_uninstall_led, char *dev_name)*/
//#define act_stub_open(a, b)  (int)stub_mod_cmd((void*)(uint32)(a), (void*)(b), (void*)(0), STUB_OP_OPEN)
struct stub_device* act_stub_open(char *dev_name, u8_t serial_num);

/*Turn off the stub and disconnect the device from the PC */
/*int stub_exit(struct stub_device* stub)*/
#define act_stub_close(d) (int)stub_mod_cmd((struct stub_device*)(d), (void*)(0), (void*)(0), (void*)(0), STUB_OP_CLOSE)

/*Read data sent by PC */
/*int stub_read(struct stub_device* stub, uint8 opcode, uint8 *data_buffer, uint32 data_len)*/
/*Opcode: protocol command word; data_ Buffer: buffer pointer to store data; data_ Len: data length */
#define stub_get_data(d, a, b, c)  (int)stub_mod_cmd((struct stub_device*)(d), (void*)(a), (void*)(b), (void*)(c), STUB_OP_READ)

/*Send data to PC */
/*int stub_write(struct stub_device* stub, uint8 opcode, uint8 *data_buffer, uint32 data_len)*/
/*Opcode: protocol command word; data_ Buffer: buffer pointer to store data; data_ Len: data length */
#define stub_set_data(d, a, b, c)  (int)stub_mod_cmd((struct stub_device*)(d), (void*)(a), (void*)(b), (void*)(c), STUB_OP_WRITE)

/*Reserved extension */
/*int stub_ioctl(struct stub_device* stub, uint8 op_code, void *param1, void *param2)*/
#define stub_ioctrl_set(d, a, b, c) (int)stub_mod_cmd((struct stub_device*)(d), (void*)(a), (void*)(b), (void*)(c), STUB_OP_IOCTL)

/*Read data sent by PC*/
/*int stub_read(struct stub_device* stub, uint8 opcode, uint8 *data_buffer, uint32 data_len)*/
/*Opcode: protocol command word; data_ Buffer: buffer pointer to store data; data_ Len: data length */
#define stub_ext_read(d, a)  (int)stub_mod_cmd((struct stub_device*)(d), (void*)(a), (void*)(0), (void*)(0), STUB_OP_EXT_RW)

/*Read data sent by PC*/
/*int stub_read(struct stub_device* stub, uint8 opcode, uint8 *data_buffer, uint32 data_len)*/
/*Opcode: protocol command word; data_ Buffer: buffer pointer to store data; data_ Len: data length */
#define stub_ext_write(d, a)  (int)stub_mod_cmd((struct stub_device*)(d), (void*)(a), (void*)(1), (void*)(0), STUB_OP_EXT_RW)

/*Read data sent by PC */
/*int stub_read(struct stub_device* stub, uint8 opcode, uint8 *data_buffer, uint32 data_len)*/
/*Opcode: protocol command word; data_ Buffer: buffer pointer to store data; data_ Len: data length */
#define stub_raw_read(d, a, b)  (int)stub_mod_cmd((struct stub_device*)(d), (void*)(a), (void*)(b), (void*)(0), STUB_OP_RAW_RW)

/*Read data sent by data PC */
/*int stub_read(struct stub_device* stub, uint8 opcode, uint8 *data_buffer, uint32 data_len)*/
/*Opcode: protocol command word; data_ Buffer: buffer pointer to store data; data_ Len: data length */
#define stub_raw_write(d, a, b)  (int)stub_mod_cmd((struct stub_device*)(d), (void*)(a), (void*)(b), (void*)(1), STUB_OP_RAW_RW)

/* Get the connection status; a: detect connection flag, 0: return the last connection status, 1: re detect the connection status; 
	Return values < = 0: not connected, > 0: connected (return UART_ Rx1: A21 or a16) */
#define stub_get_connect_state(d, a)  (int)stub_mod_cmd((struct stub_device*)(d), (void*)GET_CONNECT_STATE, (void*)(a), (void*)(0), STUB_OP_IOCTL)

/* Set LED indicator status (each bit represents one indicator); a: LED indicator number mask; B: status  */
#define stub_set_led_state(d, a, b)  (int)stub_mod_cmd((struct stub_device*)(d), (void*)SET_LED_STATE, (void*)(a), (void*)(b), STUB_OP_IOCTL)

/* Set debugging mode: 1: allow debugging printing, 0: disable debugging printing  */
#define stub_set_debug_mode(d, a)  (int)stub_mod_cmd((struct stub_device*)(d), (void*)SET_DEBUG_MODE, (void*)(a), (void*)(0), STUB_OP_IOCTL)

#endif
