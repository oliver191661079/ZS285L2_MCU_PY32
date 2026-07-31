#include <brom_interface.h>

static struct brom_operations * const rom_api = (struct brom_operations *)0x290;

void k_busy_wait(unsigned int us)
{
    rom_api->p_k_busy_wait(us);
}

void* memset(void * s, int c, unsigned int count)
{
    return rom_api->p_memset(s, c, count);
}

void* memcpy(void *dest, const void *src, unsigned int count)
{
    return rom_api->p_memcpy(dest, src, count);
}

int memcmp(const void *s1, const void *s2, unsigned int len)
{
	return rom_api->p_api_memcmp(s1, s2, len);
}

size_t strlen(const char *s)
{
	return rom_api->p_api_strlen(s);
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	return rom_api->p_api_strncmp(s1, s2, n);
}