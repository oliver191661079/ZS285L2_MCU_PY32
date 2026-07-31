
#ifndef __COMMON_H
#define __COMMON_H


#define SBC_MIPS

#include <kernel.h>
#include <stdint.h>
#include <sys/types.h>
#include <sbc_encode_interface.h>


#ifndef SBC_MIPS
#define SBC_DEBUG

typedef unsigned __int64 uint64_t;
typedef __int64  int64_t;

#else

#define CHAR_BIT      8         /* number of bits in a char */

#endif

#define int16_t short

typedef unsigned int uintptr_t;

#define SBC_ALWAYS_INLINE
#define AFMT_S16_BE 0
#define VERSION 0

#ifndef NULL
#define NULL 0
#endif

#endif




