#ifndef __NSM_SPPBLE_TEST_H__
#define __NSM_SPPBLE_TEST_H__
enum{
	RX_TYPE_IDLE,
	RX_TYPE_BR,
	RX_TYPE_EDR,
	RX_TYPE_BLE_1M,
	RX_TYPE_BLE_2M,
	RX_TYPE_MAX,
};

typedef struct{
	u8_t rx_type;
	u8_t channel;
	u8_t packet_type;
	u8_t reserved;
	u32_t rx_result[4];
} rx_res_t;

typedef struct{
	char magic[6];
	u8_t writen_bitmap;
	u8_t read_bitmap;
	rx_res_t res[8];
} nsm_res_t;

typedef struct{
	nsm_res_t res;
	struct device *dev;
	io_stream_t sppble_stream;
	os_delayed_work nsm_delay_work;
	u8_t connected : 1;
	u8_t uploaded_res : 1;

}nsm_data_t;

typedef struct {
	u16_t len;
	u16_t rid;
	u8_t  cmd[];
} nsm_cmd_t;

#define NSM_RES_ADDR_START (0x26c000)
#define NSM_MAGIC ("NSMBT")
#define NSM_CMD_STREAM_START (0x37c00)
#define NSM_CMD_STREAM_LEN (512)


#endif
