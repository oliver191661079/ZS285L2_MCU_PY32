/*
 * Copyright (c) 2010, 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Low-level debug output
 *
 * Low-level debugging output. Platform installs a character output routine at
 * init time. If no routine is installed, a nop routine is called.
 */

#include <kernel.h>
#include <misc/printk.h>

#ifndef CONFIG_USE_ROM_LIBC
#define MAX_LINE_LENGTH_BYTES (64)
#define DEFAULT_LINE_LENGTH_BYTES (16)
void print_buffer(const void *addr, int width, int count, int linelen, unsigned long disp_addr)
{
	int i, thislinelen;
	const void *data;
	/* linebuf as a union causes proper alignment */
	union linebuf {
		u32_t ui[MAX_LINE_LENGTH_BYTES/sizeof(u32_t) + 1];
		u16_t us[MAX_LINE_LENGTH_BYTES/sizeof(u16_t) + 1];
		u8_t  uc[MAX_LINE_LENGTH_BYTES/sizeof(u8_t) + 1];
	} lb;

	if (linelen * width > MAX_LINE_LENGTH_BYTES)
		linelen = MAX_LINE_LENGTH_BYTES / width;
	if (linelen < 1)
		linelen = DEFAULT_LINE_LENGTH_BYTES / width;

	if (disp_addr == -1)
		disp_addr = (unsigned long)addr;

    //这句代码是确定print_buffer的调用者
	//printk("caller %p\n", __builtin_return_address(0));

	while (count) {
		thislinelen = linelen;
		data = (const void *)addr;

		printk("%08x:", (unsigned int)disp_addr);

		/* check for overflow condition */
		if (count < thislinelen)
			thislinelen = count;

		/* Copy from memory into linebuf and print hex values */
		for (i = 0; i < thislinelen; i++) {
			if (width == 4) {
				lb.ui[i] = *(volatile u32_t *)data;
				printk(" %08x", lb.ui[i]);
			} else if (width == 2) {
				lb.us[i] = *(volatile u16_t *)data;
				printk(" %04x", lb.us[i]);
			} else {
				lb.uc[i] = *(volatile u8_t *)data;
				printk(" %02x", lb.uc[i]);
			}
			data += width;
		}

		while (thislinelen < linelen) {
			/* fill line with whitespace for nice ASCII print */
			for (i = 0; i < width * 2 + 1; i++)
				printk(" ");
			linelen--;
		}

		/* Print data in ASCII characters */
		for (i = 0; i < thislinelen * width; i++) {
			if (lb.uc[i] < 0x20 || lb.uc[i] > 0x7e)
				lb.uc[i] = '.';
		}
		lb.uc[i] = '\0';
		printk("    %s\n", lb.uc);

		/* update references */
		addr += thislinelen * width;
		disp_addr += thislinelen * width;
		count -= thislinelen;
	}
}
#endif

void print_buffer_lazy(const char* tag, const void *buffer, unsigned long count)
{
	unsigned long l;
	const char *p = buffer;

	if(tag){
		printk("%s:\n", tag);
	}

	while (count > 0 ) {
		if(count > 0x80) {
			l = 0x80;
			k_sleep(1);
		} else {
			l = count;
		}
		print_buffer(p, 1, l, 16, (u32_t)p);
		count -= l;
		p += l;
	}
}
