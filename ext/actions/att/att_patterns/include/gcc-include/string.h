#ifndef _STRING_H_
#define _STRING_H_

#include <types.h>
#include <ctype.h>

void *memset(void *dst, int val, unsigned int count);
void *memcpy(void *dest, const void *src, unsigned int count);
int memcmp(const void *s1, const void *s2, unsigned int len);
void *memmove(void *dst, const void *src, unsigned int len);
void *memchr(const void *src, int c, unsigned int len);

size_t strlen(const char * s);
int strncmp(const char *s1, const char *s2, size_t n);

#endif  /* _STRING_H_ */
