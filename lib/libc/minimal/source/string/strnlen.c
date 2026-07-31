#include <string.h>

/**
 * strnlen - Find the length of a length-limited string
 * @s: The string to be sized
 * @count: The maximum number of bytes to search
 */
size_t strnlen(const char * s, size_t count)
{
	const char *sc;

	for (sc = s; (count!=0) && (*sc != '\0'); ++sc)
	{
		count--;
	}
	return sc - s;
}


