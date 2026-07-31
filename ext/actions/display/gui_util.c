/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file gui manager interface
 */
 
#include <os_common_api.h>
#include <string.h>
#include <mem_manager.h>
#include <transcode.h>
#include <kernel.h>

int GUI_UTF8_to_Unicode(u8_t* utf8, int len, u16_t** unicode)
{
    u16_t*  p = NULL;

    int  bufSize;

    if (utf8 == NULL || unicode == NULL)
        goto err;

    if (len < 0)
        len = strlen(utf8);

    bufSize = (len + 2) * 2;

    if ((p = *unicode) == NULL)
    {
        if ((p = mem_malloc(bufSize)) == NULL) {
			goto err;
		}
    }

    if (utf8_to_unicode(utf8, len, p, &bufSize) < 0) {
		printk("GUI_UTF8_to_Unicode error \n");
		goto err;
	}


    *unicode = p;

    len = bufSize / 2;

    p[len] = '\0';  // add '\0' to tail

#if debug
	printk("utf8:%s , len %d  -> len %d  \n",utf8, strlen(utf8),len);

	printk("utf8 :\n");
	for(i = 0; i < strlen(utf8); i++)
	{
		printk("0x%x " ,utf8[i]);
	}
	printk("\nunicode :\n");
	for(i = 0; i < len; i++)
	{
		printk("0x%x " ,p[i]);
	}
	printk("\n");
#endif

    return len;
err:

	if(p){
		mem_free(p);
	}
    return -1;
}

int GUI_MBCS_to_Unicode(u8_t* mbcs, int len, u16_t** unicode)
{
    u16_t*  p = NULL;
    int  bufSize;

    if (mbcs == NULL || unicode == NULL) {
        goto err;
	}

    bufSize = (len + 2) * 2;

    if ((p = *unicode) == NULL) {
        if ((p = mem_malloc(bufSize)) == NULL) {
			goto err;
		}
    }

    if (mbcs_to_unicode(mbcs, p, len, &bufSize, -1) < 0) {
		goto err;
	}

    *unicode = p;

    len = bufSize / 2;

    p[len] = '\0';  // add '\0' to tail

    return len;
err:

	if(p){
		mem_free(p);
	}
    return -1;
}


int GUI_Unicode_to_UTF8(u16_t* unicode, int len, u8_t** utf8)
{
    char*  p;
    int  bufSize;

    if (unicode == NULL || utf8 == NULL) {
        goto err;
	}

    bufSize = (len + 2) * 3;

    if ((p = *utf8) == NULL)
    {
        if ((p = mem_malloc(bufSize)) == NULL){
			goto err;
		}
    }

    if (unicode_to_utf8(unicode, len * 2, p, &bufSize) < 0) {
		goto err;
	}

    *utf8 = p;

    len = bufSize;

    p[len] = '\0';  // add '\0' to tail

    return len;
err:

    return -1;
}


int GUI_Unicode_to_MBCS(u16_t* unicode, int len, u8_t** mbcs)
{
    char*  p;

    int  bufSize;

    if (unicode == NULL || mbcs == NULL){
		goto err;
	}

    bufSize = (len + 2) * 2;

    if ((p = *mbcs) == NULL)
    {
        if ((p = mem_malloc(bufSize)) == NULL) {
			goto err;
		}
    }

    if (unicod_to_mbcs(unicode, p, len * 2, &bufSize, -1) < 0) {
			goto err;
	}

    *mbcs = p;

    len = bufSize;

    p[len] = '\0';  // add '\0' to tail

    return len;
err:

    return -1;
}

int GUI_UTF8_to_MBCS(u8_t* utf8, int len, u8_t** mbcs)
{
    return len;
}

int GUI_MBCS_to_UTF8(u8_t* mbcs, int len, u8_t** utf8)
{

    return len;
}

int GUI_GBK_to_Unicode(u8_t* gbk, int len, u16_t** unicode)
{
	u16_t*	p = NULL;

	int  bufSize;

	if (gbk == NULL || unicode == NULL)
		goto err;

	if (len < 0)
		len = strlen(gbk);

	bufSize = (len + 2) * 2;

	if ((p = *unicode) == NULL)
	{
		if ((p = mem_malloc(bufSize)) == NULL) {
			goto err;
		}
	}

	if (gbk_to_unicode(gbk, p, &bufSize) < 0) {
		printk("GUI_UTF8_to_Unicode error \n");
		goto err;
	}

	*unicode = p;

	len = bufSize / 2;

	p[len] = '\0';	// add '\0' to tail

	return len;
err:

	if(p){
		mem_free(p);
	}
	return -1;
}
/* end of GUI_Locale.c */


