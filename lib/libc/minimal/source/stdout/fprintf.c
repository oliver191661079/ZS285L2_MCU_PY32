/* fprintf.c */

/*
 * Copyright (c) 1997-2010, 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdarg.h>
#include <stdio.h>

#define DESC(d) ((void *)d)

extern int _prf(int (*func)(), void *dest,
				const char *format, va_list vargs);

extern int vprintk(const char *fmt, va_list args);
 				

int fprintf(FILE *_MLIBC_RESTRICT F, const char *_MLIBC_RESTRICT format, ...)
{
	va_list vargs;
	int     r;

	va_start(vargs, format);
	r = _prf(fputc, DESC(F), format, vargs);
	va_end(vargs);

	return r;
}

int vfprintf(FILE *_MLIBC_RESTRICT F, const char *_MLIBC_RESTRICT format,
	     va_list vargs)
{
	int r;

	r = _prf(fputc, DESC(F), format, vargs);

	return r;
}

int printf(const char *_MLIBC_RESTRICT format, ...)
{
#if 0
	va_list vargs;
	int     r;

	va_start(vargs, format);
	r = _prf(fputc, DESC(stdout), format, vargs);
	va_end(vargs);

	return r;
#else
	int ret;
	va_list ap;

    va_start(ap, format);
    ret = vprintk(format, ap);
    va_end(ap);

	return ret;    
#endif
}

int vprintf(const char *_MLIBC_RESTRICT format, va_list vargs)
{
	int r;

	r = _prf(fputc, DESC(stdout), format, vargs);

	return r;
}
