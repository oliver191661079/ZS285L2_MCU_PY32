/* printk.h - low-level debug output */

/*
 * Copyright (c) 2010-2012, 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _PRINTK_H_
#define _PRINTK_H_

#include <toolchain.h>
#include <stddef.h>
#include <stdarg.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @brief Print kernel debugging message.
 *
 * This routine prints a kernel debugging message to the system console.
 * Output is send immediately, without any mutual exclusion or buffering.
 *
 * A basic set of conversion specifier characters are supported:
 *   - signed decimal: \%d, \%i
 *   - unsigned decimal: \%u
 *   - unsigned hexadecimal: \%x (\%X is treated as \%x)
 *   - pointer: \%p
 *   - string: \%s
 *   - character: \%c
 *   - percent: \%\%
 *
 * No other conversion specification capabilities are supported, such as flags,
 * field width, precision, or length attributes.
 *
 * @param fmt Format string.
 * @param ... Optional list of format arguments.
 *
 * @return N/A
 */
#ifdef CONFIG_PRINTK
extern __printf_like(1, 2) int printk(const char *fmt, ...);
extern __printf_like(1, 0) int vprintk(const char *fmt, va_list ap);
extern __printf_like(3, 4) int snprintk(char *str, size_t size,
					const char *fmt, ...);
extern __printf_like(3, 0) int vsnprintk(char *str, size_t size,
					  const char *fmt, va_list ap);

extern __printf_like(3, 0) void _vprintk(int (*out)(int, void *), void *ctx,
					 const char *fmt, va_list ap);

void print_buffer(const void *buffer, int width, int count, int linelen, unsigned long disp_addr);
void print_buffer_lazy(const char* tag, const void *buffer, unsigned long count);

extern __printf_like(1, 2) int printk_sync(const char *fmt, ...);

#ifdef CONFIG_ASSERT
#if defined(CONFIG_ASSERT_SHOW_FILE_FUNC)
#define ASSERT_PARAM const char *test, const char *file, const char *func, unsigned int line, const char *fmt, ...
#elif defined(CONFIG_ASSERT_SHOW_FILE) || defined(CONFIG_ASSERT_SHOW_FUNC)
#define ASSERT_PARAM const char *test, const char *str, unsigned int line, const char *fmt, ...
#else
#define ASSERT_PARAM const char *test, unsigned int line, const char *fmt, ...
#endif

extern void trace_assert(ASSERT_PARAM);

#endif

extern void log_nano(const char *module, const char *level, const char *func, const char *str, unsigned short num, const char *fmt, ...);


#else
static inline __printf_like(1, 2) int printk(const char *fmt, ...)
{
	ARG_UNUSED(fmt);
	return 0;
}

static inline __printf_like(1, 0) int vprintk(const char *fmt, va_list ap)
{
	ARG_UNUSED(fmt);
	ARG_UNUSED(ap);
	return 0;
}

static inline __printf_like(3, 4) int snprintk(char *str, size_t size,
					       const char *fmt, ...)
{
	ARG_UNUSED(str);
	ARG_UNUSED(size);
	ARG_UNUSED(fmt);
	return 0;
}

static inline __printf_like(3, 0) int vsnprintk(char *str, size_t size,
						const char *fmt, va_list ap)
{
	ARG_UNUSED(str);
	ARG_UNUSED(size);
	ARG_UNUSED(fmt);
	ARG_UNUSED(ap);

	return 0;
}

static inline void print_buffer(const void *buffer, int width, int count, int linelen,
				unsigned long disp_addr)
{
	ARG_UNUSED(buffer);
	ARG_UNUSED(width);
	ARG_UNUSED(count);
	ARG_UNUSED(linelen);
}

static inline __printf_like(1, 2) int printk_sync(const char *fmt, ...)
{
	ARG_UNUSED(fmt);
	return 0;
}

#endif

#ifdef CONFIG_PRINTK
#define TRACE_SYNC(...)     printk(__VA_ARGS__)
#define TRACE(...)          printk(__VA_ARGS__)
#define PR_EXC(...)         printk(__VA_ARGS__)
#define TRACE_ASSERT(...)   trace_assert(__VA_ARGS__)
#else
#define TRACE_SYNC(...)
#define TRACE(...)
#define PR_EXC(...)
#define TRACE_ASSERT(...)
#endif /* CONFIG_PRINTK */


#ifdef __cplusplus
}
#endif

#endif
