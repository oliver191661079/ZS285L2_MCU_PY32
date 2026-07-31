#ifndef __UTILS_H__
#define __UTILS_H__

#include "types.h"

#define CRC_HW_DMA_CHAN  0
extern void calculate_crc32_hw_init(void);
extern void calculate_crc32_hw_deinit(void);
extern unsigned int calculate_crc32(unsigned char *buffer, unsigned int buf_len,
		unsigned int crc_initial, bool last);

extern void rc4_decode(void *buf, unsigned int buf_len);

#endif /* __UTILS_H__ */
