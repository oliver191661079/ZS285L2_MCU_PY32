/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system app att
 */
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <property_manager.h>
#include <app_manager.h>
#include <input_manager.h>
#include "app_defines.h"
#include "system_app_att.h"
#include "system_app.h"

static struct device *stub_dev;
extern int trace_dma_print_set(unsigned int dma_enable);
#ifdef CONFIG_ATT_USB_STUB
static int32 pc_connect_probe(struct device *stub_dev)
{
	int8 ret;
	uint32 pc_type = 0;
	uint16 cnt = 0;

	while (1) {
		ret =
		    (int8) stub_read(stub_dev, STUB_CMD_OPEN,
				     (void *)(&pc_type), 4);
		cnt++;

		if (ret == 0) {
			if (pc_type != 0) {
				break;
			}
		}

		if (cnt > 3) {
			SYS_LOG_INF("check type over time!");
			break;
		}

		k_sleep(50);
	}

	printk("stub pc type %x\n", pc_type);
	return pc_type;
}
#endif

/*
Gets whether the stub was successfully connected
*/
int get_autotest_connect_status(void)
{
	int ret;

#ifdef CONFIG_ATT_USB_STUB
	extern uint8_t usb_phy_get_vbus(void);
	if (usb_phy_get_vbus()) {
		stub_dev = device_get_binding(CONFIG_STUB_DEV_USB_NAME);
		if (stub_dev != NULL) {
			ret = stub_open(stub_dev);
			if (ret != 0) {
				//Failed to start the USB stub
				SYS_LOG_INF("usb stub open fail %d\n", ret);
			} else {
				printk("wait usb stub drv ready\n");
				extern bool usb_stub_configured(void);
				do {
					k_sleep(100);
				}
				while (!usb_stub_configured());

				if (pc_connect_probe(stub_dev) ==
				    STUB_PC_TOOL_ATT_MODE) {
					SYS_LOG_INF("usb stub ATT\n");
					goto stub_connected;
				} else {
					printk("STUB open  not at att mode\n");
					stub_close(stub_dev);
				}
			}
		} else {
			//Stub startup failed
			printk("not found usb stub\n");
		}
	}
#endif

#ifdef CONFIG_STUB_DEV_UART

#ifdef CONFIG_ACTIONS_TRACE
	/* stub uart and trace both use uart0, forbidden trace dma mode */
	trace_dma_print_set(false);
#endif

	stub_dev = device_get_binding(CONFIG_STUB_DEV_UART_NAME);
	if (stub_dev != NULL) {
		ret = stub_open(stub_dev);
		if (ret != 0) {
#ifdef CONFIG_ACTIONS_TRACE
			/* enable dma print */
			trace_dma_print_set(true);
#endif
			//Failed to start the UART stub
			printk("failed to open UART_STUB: %d\n", ret);
		} else {
			printk("UART_STUB ATT\n");

			tool_set_dev_type(TOOL_DEV_TYPE_UART0);

			goto stub_connected;
		}

	} else {
		printk(" not found UART_STUB\n");
	}
#endif

	return -1;
 stub_connected:
	return 0;
}

int32 att_write_data(uint16 cmd, uint32 payload_len, uint8 * data_addr)
{
	int32 ret_val;
	stub_ext_param_t ext_param;

	if ((payload_len + 8) == 64) {
		payload_len += 4;
	}

	ext_param.opcode = cmd;
	ext_param.payload_len = payload_len;
	ext_param.rw_buffer = (uint8 *) data_addr;

	ret_val = stub_ext_rw(stub_dev, &ext_param, 1);	//stub_ext_write(&ext_param);

	return ret_val;
}

int32 att_read_data(uint16 cmd, uint32 payload_len, uint8 * data_addr)
{
	int32 ret_val;
	stub_ext_param_t ext_param;

	ext_param.opcode = cmd;
	ext_param.payload_len = payload_len;
	ext_param.rw_buffer = (uint8 *) data_addr;

	ret_val = stub_ext_rw(stub_dev, &ext_param, 0);	//stub_ext_read(&ext_param);

	return ret_val;
}

/**
 *  @brief  read data from atf file
 *
 *  @param  dst_addr  : buffer address
 *  @param  dst_buffer_len : buffer size
 *  @param  offset    : offset of the atf file head
 *  @param  total_len : read bytes
 *
 *  @return  negative errno code on fail, or return read bytes
 */
int read_atf_file_by_stub(u8_t * dst_addr, u32_t dst_buffer_len, u32_t offset,
			  s32_t total_len)
{
#define MAX_READ_LEN 200
	u8_t *stub_rw_buffer;
	int ret_val = 0;

	stub_rw_buffer = mem_malloc(256);

	if (!stub_rw_buffer) {
		return -ENOMEM;
	}

	atf_file_read_t *sendData = (atf_file_read_t *) stub_rw_buffer;
	atf_file_read_ack_t *recvData = (atf_file_read_ack_t *) stub_rw_buffer;

	u32_t cur_file_offset = offset;
	s32_t left_len = total_len;

	SYS_LOG_INF("data_info -> offset:%d total_len:%d\n", offset, total_len);
	while (left_len > 0) {
		u32_t read_len = left_len;
		if (read_len >= MAX_READ_LEN)
			read_len = MAX_READ_LEN;
		SYS_LOG_INF("left_len %d", left_len);
		//The size of the actual read load data must be aligned with four bytes, otherwise the read fails
		u32_t aligned_len = (read_len + 3) / 4 * 4;
		sendData->offset = cur_file_offset;
		sendData->readLen = aligned_len;

		//Send read command
		int att_data_cnt = 10;
		do {
			att_data_cnt--;
			ret_val =
			    att_write_data(STUB_CMD_ATT_FREAD,
					   (sizeof(atf_file_read_t) -
					    sizeof(stub_ext_cmd_t))
					   , (u8_t *) sendData);
			if (ret_val) {
				k_sleep(100);
			} else {
				break;
			}
		} while (att_data_cnt);

		printk("write att_data_cnt %d, line %d\n", att_data_cnt,
		       __LINE__);
		if (ret_val != 0) {
			ret_val = -1;
			break;
		}
		//read data and save to buffer
		att_data_cnt = 10;
		do {
			att_data_cnt--;
			ret_val =
			    att_read_data(STUB_CMD_ATT_FREAD, aligned_len,
					  (u8_t *) recvData);

			if (ret_val) {
				k_sleep(100);
			} else {
				break;
			}

		} while (att_data_cnt);

		printk("read att_data_cnt %d, line %d\n", att_data_cnt,
		       __LINE__);
		if (ret_val != 0) {
			ret_val = -1;
			break;
		}

		memcpy(dst_addr, recvData->payload, read_len);

		dst_addr += read_len;
		left_len -= read_len;
		cur_file_offset += read_len;
	}

	if (ret_val == 0) {
		SYS_LOG_INF("read:%d bytes ok!\n", total_len);
	}

	mem_free(stub_rw_buffer);

	return (ret_val < 0) ? ret_val : total_len;
}

/**
 *  @brief  read data from atf file
 *
 *  @param  dst_addr  : buffer address
 *  @param  dst_buffer_len : buffer size
 *  @param  offset    : offset of the atf file head
 *  @param  read_len  : read bytes
 *
 *  @return  negative errno code on fail, or return read bytes
 */
int read_atf_file(u8_t * dst_addr, u32_t dst_buffer_len, u32_t offset,
		  s32_t read_len)
{
	s32_t ret = -1;

	ret = read_atf_file_by_stub(dst_addr, dst_buffer_len, offset, read_len);

	return ret;
}

/**
 *  @brief  read the attr of one sub file in atf file by sub file name
 *
 *  @param  sub_file_name : sub file name
 *  @param  sub_atf_dir   : return parameter, the attr of the sub file
 *
 *  @return  0 on success, negative errno code on fail.
 */
int atf_read_sub_file_attr(const u8_t * sub_file_name, atf_dir_t * sub_atf_dir)
{
	int ret_val;
	int i, j, cur_files;
	int read_len;
	int atf_offset = 32;
	atf_dir_t *cur_atf_dir;
	atf_dir_t *atf_dir_buffer;

	if (NULL == sub_file_name || NULL == sub_atf_dir) {
		return -1;
	}

	atf_dir_buffer = (atf_dir_t *) app_mem_malloc(512);
	if (NULL == atf_dir_buffer) {
		return -1;
	}
	//fixme
	//i = g_atf_head.file_total;
	i = 32;
	while (i > 0) {
		if (i >= 16) {
			cur_files = 16;
			read_len = 512;
		} else {
			cur_files = i;
			read_len = i * 32;
		}

		printk("read offset %x len %x\n", atf_offset, read_len);

		ret_val =
		    read_atf_file((u8_t *) atf_dir_buffer, 512, atf_offset,
				  read_len);
		if (ret_val < read_len) {
			SYS_LOG_INF("read_atf_file fail\n");
			break;
		}

		cur_atf_dir = atf_dir_buffer;
		for (j = 0; j < cur_files; j++) {
			if (strncmp
			    (sub_file_name, cur_atf_dir->filename,
			     ATF_MAX_SUB_FILENAME_LENGTH) == 0) {
				memcpy(sub_atf_dir, cur_atf_dir,
				       sizeof(atf_dir_t));
				ret_val = 0;
				goto read_exit;
			}

			cur_atf_dir++;
		}

		atf_offset += read_len;
		i -= cur_files;
	}

	ret_val = -1;
	SYS_LOG_INF("can't find %s\n", sub_file_name);

 read_exit:
	app_mem_free(atf_dir_buffer);
	return ret_val;
}

/**
 *  @brief  read data from one sub file in atf file by name
 *
 *  @param  dst_addr  : buffer address
 *  @param  dst_buffer_len : buffer size
 *  @param  sub_file_name : sub file name
 *  @param  offset    : offset of the atf file head
 *  @param  read_len  : read bytes
 *
 *  @return  negative errno code on fail, or return read bytes
 */
int read_atf_sub_file(u8_t * dst_addr, u32_t dst_buffer_len,
		      const u8_t * sub_file_name, s32_t offset, s32_t read_len,
		      atf_dir_t * sub_atf_dir)
{
	int ret = atf_read_sub_file_attr(sub_file_name, sub_atf_dir);
	if (ret != 0)
		return -1;

	SYS_LOG_INF
	    ("sub_atf_dir offset = 0x%x, len = %x, load = %x, run = %x\n",
	     sub_atf_dir->offset, sub_atf_dir->length, sub_atf_dir->load_addr,
	     sub_atf_dir->run_addr);

	if (read_len < 0) {
		read_len = sub_atf_dir->length - offset;
	}

	if (read_len < 0) {
		SYS_LOG_INF("read_len <= 0\n");
		return -1;
	}

	if (read_len > dst_buffer_len) {
		SYS_LOG_INF("read_len:%d > dst_buffer_len:%d\n", read_len,
			    dst_buffer_len);
		return -1;
	}

	if (offset + read_len > sub_atf_dir->length) {
		SYS_LOG_INF
		    ("sub_file_name%s, offset_from_sub_file:%d + read_len:%d > sub_file_length:%d\n",
		     sub_file_name, offset, read_len, sub_atf_dir->length);
		return -1;
	}

	dst_addr = (u8_t *) sub_atf_dir->load_addr;

	return read_atf_file(dst_addr, dst_buffer_len,
			     sub_atf_dir->offset + offset, read_len);
}

//extern int bteg_set_bqb_flag(int value);
//extern int bteg_get_bqb_flag(void);

static int get_nvram_bt_name(char *name)
{
	int ret_val;

	ret_val = property_get(CFG_BT_NAME, name, TEST_BTNAME_MAX_LEN);

	return ret_val;
}

static int set_nvram_bt_name(const char *name)
{
	char cmd_data[TEST_BTNAME_MAX_LEN] = { 0 };
	int ret_val;
	int len;

	len =
	    strlen(name) + 1 >
	    TEST_BTNAME_MAX_LEN ? TEST_BTNAME_MAX_LEN : strlen(name) + 1;
	memcpy(cmd_data, name, len - 1);

	ret_val = property_set_factory(CFG_BT_NAME, cmd_data, len);
	if (ret_val < 0)
		return ret_val;

	property_flush(CFG_BT_NAME);

	return 0;
}

static int get_nvram_ble_name(char *name)
{
	int ret_val;

	ret_val = property_get(CFG_BLE_NAME, name, TEST_BTBLENAME_MAX_LEN);

	return ret_val;
}

static int set_nvram_ble_name(const char *name)
{
	char cmd_data[TEST_BTBLENAME_MAX_LEN] = { 0 };
	int ret_val;
	int len;

	len =
	    strlen(name) + 1 >
	    TEST_BTBLENAME_MAX_LEN ? TEST_BTBLENAME_MAX_LEN : strlen(name) + 1;
	memcpy(cmd_data, name, len - 1);

	ret_val = property_set_factory(CFG_BLE_NAME, cmd_data, len);
	if (ret_val < 0)
		return ret_val;

	property_flush(CFG_BLE_NAME);

	return 0;
}

static int get_nvram_bt_addr(u8_t * addr)
{
	char cmd_data[16] = { 0 };
	u8_t addr_rev[6];
	int ret_val, i;

	ret_val = property_get(CFG_BT_MAC, cmd_data, 16);

	if (ret_val < 12)
		return -1;

	str_to_hex((char *)addr_rev, cmd_data, 6);

	for (i = 0; i < 6; i++)
		addr[i] = addr_rev[5 - i];

	return 0;
}

static int set_nvram_bt_addr(const u8_t * addr)
{
	u8_t mac_str[16] = { 0 };
	u8_t addr_rev[6];
	int ret_val, i;

	for (i = 0; i < 6; i++)
		addr_rev[i] = addr[5 - i];

	hex_to_str((char *)mac_str, (char *)addr_rev, 6);

	ret_val = property_set_factory(CFG_BT_MAC, (char *)mac_str, 12);
	if (ret_val < 0)
		return ret_val;

	property_flush(CFG_BT_MAC);

	return 0;
}

static int set_nvram_bt_test_mode(u8_t* data)
{
	int ret_val;

	ret_val = property_set(CFG_BT_TEST_MODE, data, 1);
	if (ret_val < 0) {
		return ret_val;
	}
	property_flush(CFG_BT_TEST_MODE);

	return 0;
}

// return 0: success, or failed
int get_nvram_db_data(u8_t db_data_type, u8_t * data, u32_t len)
{
	int ret_val;

	if (db_data_type == DB_DATA_BT_ADDR) {
		ret_val = get_nvram_bt_addr(data);
	} else if (db_data_type == DB_DATA_BT_NAME) {
		ret_val = get_nvram_bt_name(data);
		if (ret_val > 0)
			ret_val = 0;

	} else if (db_data_type == DB_DATA_BT_BLE_NAME) {
		ret_val = get_nvram_ble_name(data);
		if (ret_val > 0)
			ret_val = 0;

	} else if (db_data_type == DB_DATA_BT_RF_BQB_FLAG) {
		return -1;
	} else {
		return -1;
	}

	if (ret_val != 0) {
		printk("%s data_type %d ret_val %d:%d\n", __func__,
		       db_data_type, ret_val, len);
		return -1;
	}
	return 0;
}

// return 0: success, or failed
int set_nvram_db_data(u8_t db_data_type, u8_t * data, u32_t len)
{
	int ret_val;
	if (db_data_type == DB_DATA_BT_ADDR) {
		ret_val = set_nvram_bt_addr(data);
	} else if (db_data_type == DB_DATA_BT_NAME) {
		ret_val = set_nvram_bt_name(data);
	} else if (db_data_type == DB_DATA_BT_BLE_NAME) {
		ret_val = set_nvram_ble_name(data);
	} else if (db_data_type == DB_DATA_BT_RF_BQB_FLAG) {
		ret_val = set_nvram_bt_test_mode(data);
	} else {
		ret_val = -1;
	}

	if (ret_val < 0) {
		printk("%s data_type %d ret_val %d\n", __func__, db_data_type,
		       ret_val);
	}
	return ret_val;
}

/*
system app notify bt engine ready in MSG_BT_ENGINE_READY
*/
static bool bt_eg_ready = FALSE;
static bool try_start_eg = FALSE;
int act_att_notify_bt_engine_ready(void)
{
	os_sleep(50);
	printk("%s att bt_eg_start already\n", __func__);
	bt_eg_ready = TRUE;
	os_sleep(50);
	return 0;
}

void act_test_set_btstack_reconnect(u8_t * addr)
{
	struct autoconn_info info[3];

	memset(info, 0, sizeof(info));
	memcpy(info[0].addr.val, addr, sizeof(info[0].addr.val));
	info[0].addr_valid = 1;
	info[0].tws_role = 0;
	info[0].a2dp = 1;
	info[0].hfp = 0;
	info[0].avrcp = 1;
	info[0].hfp_first = 0;
	btif_br_set_auto_reconnect_info(info, 3);
	property_flush(NULL);

	bt_manager_manual_reconnect();
}

void bt_manager_btstack_install(u8_t * addr, u8_t test_mode)
{
	struct app_msg msg = { 0 };

	try_start_eg = TRUE;

	msg.type = MSG_AUTOTEST_START_BT;
	while (send_async_msg(CONFIG_FRONT_APP_NAME, &msg) == false) {
		k_sleep(500);
		SYS_LOG_WRN("att send to main stop bt fail,try again\n");
	}
	printk("%s wait bt_eg_ready\n\n", __func__);

	while (!bt_eg_ready) {
		k_sleep(50);
	}

	act_test_set_btstack_reconnect(addr);
}

void bt_manager_btstack_uninstall(u8_t * addr, u8_t test_mode)
{
	;
}

int att_flash_erase(off_t offset, size_t size)
{
	struct device *nor_device;
	nor_device = device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);
	if (!nor_device) {
		return -1;
	}

	// flash_write_protection_set(flash_device, false);
	flash_write_protection_region_set(nor_device, FLASH_PROTECT_REGION_NONE);

	printk("%s earse %s \n offset %d, size %d\n", __func__,
	       CONFIG_XSPI_NOR_ACTS_DEV_NAME, offset, size);

	return flash_erase(nor_device, offset, size);
}

int get_BQB_info(void)
{
#ifdef CONFIG_BT_CONTROLER_BQB
	return 1;
#else
	return 0;
#endif
}

void enter_BQB_mode(void)
{
#ifdef CONFIG_BT_CONTROLER_BQB
	extern void BT_DutTest(int bqb_mode);
	k_thread_priority_set(k_current_get(), 0);
	BT_DutTest(1);		/*1-classic bqb, 2-ble bqb */
#else
	printk("%s   BQB mode is not enable in FW !!!!!\n", __func__);
	while (1) {
		k_sleep(10);
	}
#endif
}

extern void sys_pm_reboot(int type);
const att_interface_api_t att_api = {
	.version = 0x01,

	.att_write_data = att_write_data,
	.att_read_data = att_read_data,

	.vprintk = vprintk,

	.k_uptime_get_32 = k_uptime_get_32,

	.k_sem_init = k_sem_init,

	.k_sem_give = k_sem_give,

	.k_sem_take = k_sem_take,

	.z_malloc = mem_malloc,

	.free = mem_free,

	.k_sleep = k_sleep,

	.k_thread_create = (void *)k_thread_create,

	.hal_aout_channel_open = hal_aout_channel_open,
	.hal_aout_channel_close = hal_aout_channel_close,
	.hal_aout_channel_write_data = hal_aout_channel_write_data,
	.hal_aout_channel_start = hal_aout_channel_start,
	.hal_aout_channel_stop = hal_aout_channel_stop,

	.hal_ain_channel_open = hal_ain_channel_open,
	.hal_ain_channel_close = hal_ain_channel_close,
	.hal_ain_channel_read_data = hal_ain_channel_read_data,
	.hal_ain_channel_start = hal_ain_channel_start,
	.hal_ain_channel_stop = hal_ain_channel_stop,

	.bt_manager_get_rdm_state = btif_br_get_dev_rdm_state,
	.att_btstack_install = bt_manager_btstack_install,
	.att_btstack_uninstall = bt_manager_btstack_uninstall,
	.bt_manager_call_accept = bt_manager_call_accept,

	.get_nvram_db_data = get_nvram_db_data,
	.set_nvram_db_data = set_nvram_db_data,

	.freq_compensation_read = freq_compensation_read,

	.freq_compensation_write = freq_compensation_write,

	.read_file = read_atf_sub_file,
	.print_buffer = print_buffer,
//      .bt_manager_set_not_linkkey_connect = bt_manager_set_not_linkkey_connect,
	.sys_pm_reboot = sys_pm_reboot,
	.att_flash_erase = att_flash_erase,
	.board_audio_device_mapping = board_audio_device_mapping,
	.enter_BQB_mode = enter_BQB_mode,
	.get_BQB_info = get_BQB_info,
};

/*
att At the end of the test, it is decided whether to shut down, restart, or boot directly
*/
void test_exit(void)
{
	property_flush(NULL);
#ifdef CONFIG_ATT_CYCLIC_TEST
	os_sleep(3000);
	system_power_reboot(REBOOT_TYPE_NORMAL);
#else
	sys_pm_poweroff();
	sys_event_send_message(MSG_POWER_OFF);
#endif
}

typedef struct {
	stub_ext_cmd_t cmd;
	uint8 log_data[0];
} print_log_t;
static inline int isdigit(int a)
{
	return (((unsigned)(a) - '0') < 10);
}

#define PRINT_BUF_SIZE 128

#define _SIGN     1		// signed
#define _ZEROPAD  2		// 0 prefix
#define _LARGE    4		// capitals.

#define _putc(_str, _end, _ch) \
do                             \
{                              \
    *_str++ = _ch;             \
}                              \
while (0)

const char digits[16] = "0123456789abcdef";

/*!
 * \brief Formats the output to a string (specifying the size and parameter list).
 */
static int _vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	char *str = buf;
	char *end = buf + size - 1;

	ARG_UNUSED(end);

	for (; *fmt != '\0'; fmt++) {
		uint32 flags;
		int width;

		uint32 number;
		uint32 base;

		char num_str[16];
		int num_len;
		int sign;
		uint8 ch;
		if (*fmt != '%') {
			_putc(str, end, *fmt);
			continue;
		}

		fmt++;

		flags = 0, width = 0, base = 10;

		if (*fmt == '0') {
			flags |= _ZEROPAD;
			fmt++;
		}

		while (isdigit(*fmt)) {
			width = (width * 10) + (*fmt - '0');
			fmt++;
		}

		switch (*fmt) {
		case 'c':
			{
				ch = (uint8) va_arg(args, int);

				_putc(str, end, ch);
				continue;
			}

		case 's':
			{
				char *s = va_arg(args, char *);

				while (*s != '\0')
					_putc(str, end, *s++);

				continue;
			}

			//case 'X':
			//    flags |= _LARGE;

		case 'x':
			//  case 'p':
			base = 16;
			break;

		case 'd':
			//  case 'i':
			flags |= _SIGN;

			//  case 'u':
			break;

		default:
			continue;
		}

		number = va_arg(args, uint32);

		sign = 0, num_len = 0;

		if (flags & _SIGN) {
			if ((int)number < 0) {
				number = -(int)number;

				sign = '-';
				width -= 1;
			}
		}

		if (number == 0) {
			num_str[num_len++] = '0';
		} else {

			while (number != 0) {
				char ch = digits[number % base];

				num_str[num_len++] = ch;
				number /= base;
			}
		}

		width -= num_len;

		if (sign != 0)
			_putc(str, end, sign);

		if (flags & _ZEROPAD) {
			while (width-- > 0)
				_putc(str, end, '0');
		}

		while (num_len-- > 0)
			_putc(str, end, num_str[num_len]);
	}

	*str = '\0';

	return (str - buf);
}

#define STUB_CMD_ATT_PRINT_LOG          0x0407
#define STUB_CMD_ATT_ACK                0x04fe

int print_log(const char *format, ...)
{
	int trans_bytes;

	print_log_t *print_log;

	va_list args;

	uint8 data_buffer[256];

	va_start(args, format);

	print_log = (print_log_t *) data_buffer;

	trans_bytes =
	    _vsnprintf((char *)&(print_log->log_data), PRINT_BUF_SIZE, format,
		       args);

	//Add end character
	memset(&print_log->log_data[trans_bytes], 0, 4);
	trans_bytes += 1;

	//Guarantee four-byte alignment
	trans_bytes = (((trans_bytes + 3) >> 2) << 2);

	att_write_data(STUB_CMD_ATT_PRINT_LOG, trans_bytes, data_buffer);

	printk("%s\n", (uint8 *) & (print_log->log_data));

	//Due to the UDA USB bug, this reply cannot be removed, otherwise the UDA USB data will appear offset
	att_read_data(STUB_CMD_ATT_ACK, 0, data_buffer);

	return 0;
}

static int (*att_code_entry)(void *p1, void *p2, void *p3);
//extern const struct input_app_key_tbl btapp_keymap[];
static att_interface_dev_t dev_tbl;

static void autotest_main_start(void *p1, void *p2, void *p3)
{
	dev_tbl.gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (att_code_entry) {
		att_code_entry(p1, p2, &dev_tbl);
	}
}

bool autotest_key_event_handle(int key_event, int event_stage)
{
	bool result = true;

	if (event_stage == KEY_EVENT_PRE_DONE) {
		return false;
	}

	if (app_manager_get_apptid(APP_ID_ATT) == NULL) {
		return false;
	}

	// Bluetooth push button processing
	//result = app_key_deal(app_manager_get_current_app(), key_event, btapp_keymap, bt_manager_get_status);

	return result;
}

static struct k_thread att_thread;
static u8_t *att_stack = NULL;

void autotest_start(void)
{
	int ret_val;

	atf_dir_t sub_atf_dir;

	print_log(" ");
	print_log(" ");
	print_log(" ");
	print_log(" ");
	print_log("ATT test start\n");

	ret_val =
	    read_atf_sub_file(NULL, 0x1000, "loader.bin", 0, -1, &sub_atf_dir);
	if (ret_val <= 0) {
		return;
	}

	print_log("code entry %x size %d\n", sub_atf_dir.run_addr,
		  sub_atf_dir.length);

	att_code_entry =
	    (int (*)(void *, void *, void *))((void *)(sub_atf_dir.run_addr));
	if (!att_code_entry) {
		return;
	}

#ifdef CONFIG_PLAYTTS
	tts_manager_lock();
#endif

	// GPIO testing requires active JTAG shutdown
#ifdef CONFIG_CPU0_EJTAG_ENABLE
	soc_debug_disable_jtag(SOC_JTAG_CPU0);
#endif

#ifdef CONFIG_DSP_EJTAG_ENABLE
	soc_debug_disable_jtag(SOC_JTAG_DSP);
#endif

	att_stack = malloc(ATT_STACK_SIZE);

	if (att_stack == NULL) {
		printk("att stack is null \n");
		return;
	}

	//      app_manager_active_app(APP_ID_ATT);
	k_thread_create(&att_thread, (k_thread_stack_t) att_stack,
			ATT_STACK_SIZE,
			autotest_main_start, (void *)&att_api,
			(void *)test_exit, NULL, CONFIG_APP_PRIORITY, 0, 0);

	//release main thread to receive msg
	while (!try_start_eg) {
		os_sleep(50);
	}
}

//APP_DEFINE(att, att_stack_area, sizeof(att_stack_area), CONFIG_APP_PRIORITY,
//         RESIDENT_APP, (void *)&att_api, (void *)test_exit, NULL,
//         autotest_main_start, autotest_key_event_handle);
