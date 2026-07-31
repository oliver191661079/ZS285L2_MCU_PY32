/** @file
 *  @brief Bluetooth Parse vcrad
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __BT_PARSE_VCARD_H
#define __BT_PARSE_VCARD_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
	VCARD_TYPE_NAME,
	VCARD_TYPE_TEL,
};

struct parse_vcard_result {
	uint8_t type;
	uint16_t len;
	uint8_t *data;
};

uint16_t bt_parse_one_vcard(uint8_t *data, uint16_t len, struct parse_vcard_result *result, uint8_t *result_size);

#ifdef __cplusplus
}
#endif
#endif /* __BT_PARSE_VCARD_H */
