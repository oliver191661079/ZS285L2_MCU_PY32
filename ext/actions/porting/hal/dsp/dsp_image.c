/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <os_common_api.h>
#include <dsp.h>
#include <string.h>
#include <mem_manager.h>
#include <sdfs.h>
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "DSP"
#include <logging/sys_log.h>

const struct dsp_imageinfo *dsp_create_image(const char *name)
{
	struct dsp_imageinfo *image = mem_malloc(sizeof(*image));
	if (image == NULL)
		return NULL;

	if (sd_fmap(name, (void **)&image->ptr, (int *)&image->size)) {
		SYS_LOG_ERR("cannot find dsp image \"%s\"", name);
		mem_free(image);
		return NULL;
	}

	//strncpy(image->name, name, sizeof(image->name));
	image->name = name;
	return image;
}

void dsp_free_image(const struct dsp_imageinfo *image)
{
	mem_free((void*)image);
}

uint32_t dsp_image_version_dump(const char *name)
{
	uint32_t *addr;
	uint32_t size;

	if (sd_fmap(name, (void **)&addr, (int *)&size)) {
		return 0;
	} else {
		uint8_t *p = (uint8_t *) addr;
		printk("dsp image %s: version %x.%02x%02x\n", name, addr[3], p[10], p[11]);
		return addr[3];
	}
}
