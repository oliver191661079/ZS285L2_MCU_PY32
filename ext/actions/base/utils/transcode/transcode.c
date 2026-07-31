/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file property manager interface
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <version.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef CONFIG_FILE_SYSTEM
#include <fs.h>
extern WCHAR ff_convert (WCHAR chr, UINT dir);
#endif

int gbk_to_utf8(unsigned char *lpGBKStr, unsigned char *lpUTF8Str, int len)
{
	uint16_t w ;
	uint16_t wchar ;
	len = 0;
	while (*lpGBKStr)
	{
		w = *lpGBKStr++;
		w &= 0xFF;
		if (((BYTE)(w) >= 0x81 && (BYTE)(w) <= 0xFE)) {
			w = (w << 8) + *lpGBKStr++;
		}

#ifdef CONFIG_FILE_SYSTEM
		wchar = ff_convert(w, 1);/* Convert ANSI/OEM to Unicode */
#endif
	    if (wchar <= 0x7F )
	    {
	        lpUTF8Str[len] = (char)wchar;
	        len++;
	    }
	    else if (wchar >=0x80 && wchar <= 0x7FF)
	    {
	        lpUTF8Str[len] = 0xc0 |((wchar >> 6)&0x1f);
	        len++;
	        lpUTF8Str[len] = 0x80 | (wchar & 0x3f);
	        len++;
	    }
	    else if (wchar >=0x800 && wchar < 0xFFFF)
	    {
	        lpUTF8Str[len] = 0xe0 | ((wchar >> 12)&0x0f);
	        len++;
	        lpUTF8Str[len] = 0x80 | ((wchar >> 6) & 0x3f);
	        len++;
	        lpUTF8Str[len] = 0x80 | (wchar & 0x3f);
	        len++;
	    }
	    else
	    {
	        return -1;
	    }

	}
    lpUTF8Str[len]= 0;
    return len;
}

int utf8_to_gbk(unsigned char *lpUTF8Str,unsigned char *lpGBKStr, int len)
{
	int outputSize = 0;
	char * lpUTF8Str_start = lpUTF8Str;
	char high, middle, low;
	unsigned short unicode, gbkcode;

	while (*lpUTF8Str && (lpUTF8Str <= (unsigned char *)(lpUTF8Str_start + len)))
	{
		if (*lpUTF8Str > 0x00 && *lpUTF8Str <= 0x7F)
		{
			*lpGBKStr++ = *lpUTF8Str++;
			outputSize++;
		}
		else if (((*lpUTF8Str) & 0xE0) == 0xC0)
		{
			char high = *lpUTF8Str++;
			char low = *lpUTF8Str++;
			if ((low & 0xC0) == 0x80) {
				unicode = high&0xF;
				unicode <<= 6;
				unicode |= low&0x3F;
			#ifdef CONFIG_FILE_SYSTEM
				gbkcode = ff_convert(unicode, 0);
			#endif
				if (gbkcode != 0) {
					high = (char)(gbkcode >> 8);
					low = (char)gbkcode & 0xFF;
				}
			}

			*lpGBKStr++ = high;
			*lpGBKStr++ = low;
			outputSize += 2;
		}
		else if (((*lpUTF8Str) & 0xF0) == 0xE0)
		{
			high = *lpUTF8Str++;
			middle = *lpUTF8Str++;
			low = *lpUTF8Str++;

			if (((middle & 0xC0) == 0x80) && ((low & 0xC0) == 0x80))
			{
				unicode = high & 0xF;
				unicode <<= 6;
				unicode |= middle & 0x3F;
				unicode <<= 6;
				unicode |= low & 0x3F;
			#ifdef CONFIG_FILE_SYSTEM
				gbkcode = ff_convert(unicode, 0);
			#endif
				if (gbkcode != 0) {
					*lpGBKStr++ = (char)(gbkcode >> 8);
					*lpGBKStr++ = (char)(gbkcode & 0xFF);
					outputSize += 2;
					continue;
				}
			}

			*lpGBKStr++ = high;
			*lpGBKStr++ = middle;
			*lpGBKStr++ = low;
			outputSize += 3;
		}
		else
		{
			*lpGBKStr++ = *lpUTF8Str++;
			outputSize++;
		}
	}
	return outputSize;
}
int gbk_to_unicode(unsigned char *lpGBKStr, unsigned short *unicode, int *punicode_len)
{
	uint16_t w;
	uint16_t wchar;
	int len = 0;

    if ((lpGBKStr == NULL) || (unicode == NULL) || (punicode_len == NULL)) {
        return -1;
    }

	while (*lpGBKStr)
	{
		w = *lpGBKStr++;
		w &= 0xFF;
		if (((BYTE)(w) >= 0x81 && (BYTE)(w) <= 0xFE)) {
			w = (w << 8) + *lpGBKStr++;
		}

#ifdef CONFIG_FILE_SYSTEM
		wchar = ff_convert(w, 1);/* Convert ANSI/OEM to Unicode */
#else
		wchar = w;
#endif
		*unicode = wchar;
		unicode++;
		len += 2;
	}
	*punicode_len = len;
    return 0;
}

int utf8_to_unicode(unsigned char *utf8, int utf8_len, unsigned short *unicode, int *punicode_len)
{
    int count = 0;
    unsigned char c0, c1;
    unsigned long scalar;

    if ((unicode == NULL) || (utf8 == NULL) || (punicode_len == NULL)) {
        return -1;
    }

    --utf8_len;
    while (utf8_len >= 0) {
        c0 = (unsigned char)*utf8;
        utf8++;
        if (c0 < 0x80) {
            *unicode = c0;
            unicode++;
            count += 2;
            utf8_len--;
            continue;
        }

        if ((c0 & 0xc0) == 0x80) {
            return -1;
        }
        
        scalar = c0;
        utf8_len--;
        if (utf8_len < 0) {
            return -1;
        }   
        
        c1 = (unsigned char)*utf8;
        utf8++;

        if ((c1 & 0xc0) != 0x80) {
            return -1;
        }

        scalar <<= 6;
        scalar |= (c1 & 0x3f);        

        if (!(c0 & 0x20)) {
            if ((scalar != 0) && (scalar < 0x80)){
                return -1;
            }
            *unicode = (unsigned short)scalar & 0x7ff;
            unicode++;
            count += 2;
            utf8_len--;
            continue;
        }
        
        utf8_len--;
        if (utf8_len < 0) {
            return -1;
        }
        
        c1 = (unsigned char)*utf8;
        utf8++;
      
        if ((c1 & 0xc0) != 0x80) {
            return -1;
        }

        scalar <<= 6;
        scalar |= (c1 & 0x3f);        
        
        if (!(c0 & 0x10)) {
            if (scalar < 0x800) {
                return -1;
            }
            if ((scalar >= 0xd800) && (scalar < 0xe000)) {
                return -1;
            }
            *unicode = (unsigned short)(scalar & 0x0ffff);
            unicode++;
            count += 2;
            utf8_len--;
            continue;
        }  
        return -1;
    }
    
    *punicode_len = count;
    return 0;
}

static int encode_utf8(unsigned char *s, unsigned short widechar) 
{
	int r;

	if ((widechar & 0xF800) > 0) {
		r = 3;/* Single byte (ASCII)  */
	} else if ((widechar & 0xFF80) > 0) {
		r = 2;/* Double byte sequence */
	} else {          
		r = 1;/* 3 byte sequence      */
	}

	//printf("widechar=%#x r=%d\n", widechar, r);
	switch ( r ) {
		case 1:
		*s = (unsigned char)widechar;
		break;
		case 2:
		*s = (unsigned char)(0xC0 | (widechar >> 6));
		s++;
		*s   = (unsigned char)(0x80 | (widechar & 0x3F));
		break;
		case 3:
		*s = (unsigned char)(0xE0 | (widechar >> 12));
		s++;
		*s = (unsigned char)(0x80 | ((widechar >> 6) & 0x3F));
		s++;
		*s   = (unsigned char)(0x80 | (widechar & 0x3F));
		break;
		default:
		; //
	}
	return r;
}

int unicode_to_utf8(unsigned short *unicode, int unicode_len, unsigned char *utf8, int *putf8_len)
{
    int buffersize;
    int utf8bytes = 0;
    int utf8Total = 0;

    if ((unicode == NULL) || (utf8 == NULL) || (putf8_len == NULL)) {
        return -1;
    }

    buffersize = *putf8_len;
    while ((unicode_len != 0) && (*unicode != 0))
    {
        unicode_len -= 2;
        
        utf8bytes = encode_utf8(utf8, *unicode);
        unicode++;
        
        if ((utf8Total + utf8bytes) > buffersize)
        {
            return -1;
        }
        utf8 += utf8bytes;
        utf8Total += utf8bytes;
    }   
    
    *utf8 = 0;
    *putf8_len = utf8Total;
    
    return 1;
}

int mbcs_to_unicode(unsigned char *src_mbcs, unsigned short *dst_uni,
					 int src_len, int *dst_len, int nation_id)
{
	uint16_t w ;
	uint16_t wchar ;
	uint16_t unicode_num = 0;
	int ret = 0;

    if ((src_mbcs == NULL) || (dst_uni == NULL) || (dst_len == NULL)) {
        return -1;
    }

	while (*src_mbcs) {
		w = *src_mbcs++;
		w &= 0xFF;
		if (((BYTE)(w) >= 0x81 && (BYTE)(w) <= 0xFE)) {
			w = (w << 8) + *src_mbcs++;
		}
	#ifdef CONFIG_FILE_SYSTEM
		wchar = ff_convert(w, 1);/* Convert ANSI/OEM to Unicode */
	#endif
		dst_uni[unicode_num++] = wchar;
	}

	*dst_len = unicode_num * 2;

     return ret;
}
int unicod_to_mbcs(unsigned short *src_ucs, unsigned char *dst_mbcs,
                          int src_len, int *dst_len, int nation_id)
{
    unsigned int code;
    unsigned short cu;

    int s_cnt = 0;
    int d_cnt = 0;
    int num_bytes = 0;
    int ret = 0;

    if ((src_ucs == NULL) || (dst_mbcs == NULL) || (dst_len == NULL))
    {
        return -1;
    }

    src_len = src_len/2;
    ret = 1;
    s_cnt = 0;
    for (d_cnt = 0; ((s_cnt < src_len) && (src_ucs[s_cnt] != 0));) {
        cu = src_ucs[s_cnt];
        s_cnt++;
        if (cu < 0x80) {
            dst_mbcs[d_cnt] = (unsigned char)(0x00ff & cu);
            d_cnt++;
            num_bytes++;
        } else {
		#ifdef CONFIG_FILE_SYSTEM
            code = ff_convert(cu, 0);/* Convert Unicode to ANSI/OEM  */
		#endif
            if (code == 0) {
                ret = 0;
                code = 0x20;
            }

            dst_mbcs[d_cnt] = (unsigned char)(0xff & code);
            d_cnt++;
            num_bytes++;

            if (((unsigned char) code > 0x7f) && (0 != ((unsigned char) (code >> 8)))) {
                dst_mbcs[d_cnt] = (unsigned char)((0xff00 & code) >> 8);
                d_cnt++;
                num_bytes++;
            }
        }
    }
    dst_mbcs[d_cnt] = 0;
    num_bytes++;

    *dst_len = num_bytes;

    return ret;
}
