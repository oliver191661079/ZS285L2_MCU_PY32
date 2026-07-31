#include <string.h>
#include <errno.h>
#include <soc.h>
#include <tflv/tflv.h>
#include <logging/sys_log.h>

bool tflv_find_block(u8_t *data, u16_t len, u8_t type, u8_t **addr, u16_t *size)
{
	TFLV_HEAD *h;
	u16_t offset;
	bool ret = FALSE;

	//Check file tag.
	if (data[3] != 'A' || data[2] != 'S' || data[1] != 'Q' ||
	    data[0] != 'T') {
		SYS_LOG_WRN("Wrong efx tag.");
		return ret;
	}

	offset = FILE_TAG_LEN;

	while (offset < len) {
		h = (TFLV_HEAD *)(data + offset);
		if (h->type == type) {
			*addr =  data + offset;
			*size = h->len + TFLV_HEAD_SIZE;
			ret = TRUE;
			break;
		}
		offset += sizeof(TFLV_HEAD) + h->len;
	}

	return ret;
}

u8_t* tflv_get_hfp_addr(u8_t *data, u16_t len)
{
	u8_t* addr; 
	u16_t size;

	if (tflv_find_block(data, len, AF_HFP, &addr, &size)) {
		return addr;
	} else {
		SYS_LOG_WRN("Failed.");
	}

	return NULL;
}

u16_t tflv_get_hfp_size(u8_t *data, u16_t len)
{
	u8_t* addr; 
	u16_t size;

	if (tflv_find_block(data, len, AF_HFP, &addr, &size)) {
		return size;
	} else {
		SYS_LOG_WRN("Failed.");
	}

	return 0;
}

u8_t* tflv_get_volume_table_addr(u8_t *data, u16_t len)
{
	u8_t* addr; 
	u16_t size;

	if (tflv_find_block(data, len, AF_VOL_TABLE, &addr, &size)) {
		return addr;
	} else {
		SYS_LOG_WRN("Failed.");
	}

	return NULL;
}

u16_t tflv_get_volume_table_size(u8_t *data, u16_t len)
{
	u8_t* addr; 
	u16_t size;

	if (tflv_find_block(data, len, AF_VOL_TABLE, &addr, &size)) {
		return size;
	} else {
		SYS_LOG_WRN("Failed.");
	}

	return 0;
}
