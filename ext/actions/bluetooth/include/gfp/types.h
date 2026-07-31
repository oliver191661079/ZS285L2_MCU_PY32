#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdarg.h>

#ifndef _ASSEMBLER_

#ifndef u8
typedef unsigned char u8;
#endif

#ifndef u16
typedef unsigned short u16;
#endif

#ifndef u32
typedef unsigned int u32;
#endif

#ifndef uchar
#define  uchar unsigned char
#endif

#ifndef uint8
typedef unsigned char uint8;
#endif

#ifndef int8
typedef signed char int8;
#endif

#ifndef uint16
typedef unsigned short uint16;
#endif

#ifndef int16
typedef signed short int16;
#endif

#ifndef uint32
typedef unsigned int uint32;
#endif

#ifndef int32
typedef signed int int32;
#endif

#ifndef ulong32
typedef unsigned long ulong32;
#endif

#ifndef long32
typedef signed long long32;
#endif


/* general data types
 */
typedef unsigned char uchar_t;
typedef unsigned short ushort_t;
typedef unsigned int uint_t;
typedef unsigned long ulong_t;

typedef unsigned char u8_t;
typedef unsigned short u16_t;
typedef unsigned int u32_t;
typedef unsigned long long u64_t;

typedef signed char s8_t;
typedef signed short s16_t;
typedef signed int s32_t;
typedef signed long long s64_t;

typedef u8_t byte_t;

/* volatile data types
 */
typedef volatile u8_t vu8_t;
typedef volatile u16_t vu16_t;
typedef volatile u32_t vu32_t;

/* register data types
 */
#define __reg8(_reg)   (*(vu8_t *)(_reg))
#define __reg16(_reg)  (*(vu16_t *)(_reg))
#define __reg32(_reg)  (*(vu32_t *)(_reg))

/* stdbool data types
 */
#define __defined_stdbool  1

/* stdint data types
 */
#define __defined_stdint  1

typedef s8_t int8_t;
typedef s16_t int16_t;
typedef s32_t int32_t;
typedef s64_t int64_t;

typedef u8_t uint8_t;
typedef u16_t uint16_t;
typedef u32_t uint32_t;
typedef u64_t uint64_t;


#endif /* _ASSEMBLER_ */

#endif /* _TYPES_H_ */
