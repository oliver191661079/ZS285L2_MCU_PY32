#ifndef _TFLV_H_
#define _TFLV_H_

#define HFP_PARAM_LEN 0x260
#define FILE_TAG 0x00
#define FILE_TAG_LEN 8

typedef enum {
	AF_HFP = 0,
	AF_VOL_TABLE = 1,
	AF_VOL_CONTROL = 2,
} ASQT_FLAG;

typedef struct _TFLV_
{
    u8_t type;
    u8_t flag;
    u16_t len;
}TFLV_HEAD;

#define TFLV_HEAD_SIZE 4

bool tflv_find_block(u8_t *data, u16_t len, u8_t type, u8_t **addr, u16_t *size);
u8_t* tflv_get_hfp_addr(u8_t *data, u16_t len);
u16_t tflv_get_hfp_size(u8_t *data, u16_t len);
u8_t* tflv_get_volue_table_addr(u8_t *data, u16_t len);
u16_t tflv_get_volue_table_size(u8_t *data, u16_t len);

#endif