/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file hex string trancode  interface
 */
#include <ctype.h>
#include "hex_str.h"

size_t hex_to_str(char *pszDest, char *pbSrc, int nLen)
{
	char ddl, ddh;
	for (int i = 0; i < nLen; i++) {
		ddh = 48 + ((unsigned char)pbSrc[i]) / 16;
		ddl = 48 + ((unsigned char)pbSrc[i]) % 16;

		if (ddh > 57) ddh = ddh + 7;
		if (ddl > 57) ddl = ddl + 7;

		pszDest[i * 2] = ddh;
		pszDest[i * 2 + 1] = ddl;
	}
	pszDest[nLen * 2] = '\0';
	return 2 * nLen;
}

void str_to_hex(char *pbDest, char *pszSrc, int nLen)
{
	char h1, h2;
	char s1, s2;
	for (int i = 0; i < nLen; i++) {
		h1 = pszSrc[2 * i];
		h2 = pszSrc[2 * i + 1];

		s1 = toupper(h1) - 0x30;
		if (s1 > 9)	s1 -= 7;

		s2 = toupper(h2) - 0x30;
		if (s2 > 9)	s2 -= 7;

		pbDest[i] = ((s1 << 4) | s2);
	}
}

int char2hex(char c, uint8_t *x)
{
	if (c >= '0' && c <= '9') {
		*x = c - '0';
	} else if (c >= 'a' && c <= 'f') {
		*x = c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		*x = c - 'A' + 10;
	} else {
		return -EINVAL;
	}

	return 0;
}

int hex2char(uint8_t x, char *c)
{
	if (x <= 9) {
		*c = x + '0';
	} else  if (x <= 15) {
		*c = x - 10 + 'a';
	} else {
		return -EINVAL;
	}

	return 0;
}

uint8_t u8_to_dec(char *buf, uint8_t buflen, uint8_t value)
{
	uint8_t divisor = 100;
	uint8_t num_digits = 0;
	uint8_t digit;

	while (buflen > 0 && divisor > 0) {
		digit = value / divisor;
		if (digit != 0 || divisor == 1 || num_digits != 0) {
			*buf = (char)digit + '0';
			buf++;
			buflen--;
			num_digits++;
		}

		value -= digit * divisor;
		divisor /= 10;
	}

	if (buflen) {
		*buf = '\0';
	}

	return num_digits;
}

