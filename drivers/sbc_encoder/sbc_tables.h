/*
 *
 *  Bluetooth low-complexity, subband codec (SBC) library
 *
 *  Copyright (C) 2008-2010  Nokia Corporation
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2004-2005  Henryk Ploetz <henryk@ploetzli.ch>
 *  Copyright (C) 2005-2006  Brad Midgley <bmidgley@xmission.com>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include "common.h"
#include "sbc_encode.h"

/* A2DP specification: Appendix B, page 69 */
extern const int sbc_offset4[4][4];

/* A2DP specification: Appendix B, page 69 */
extern const int sbc_offset8[4][8];

/* extra bits of precision for the synthesis filter input data */
#define SBCDEC_FIXED_EXTRA_BITS 2

#define SS4(val) ASR(val, SCALE_SPROTO4_TBL)
#define SS8(val) ASR(val, SCALE_SPROTO8_TBL)
#define SN4(val) ASR(val, SCALE_NPROTO4_TBL + 1 + SBCDEC_FIXED_EXTRA_BITS)
#define SN8(val) ASR(val, SCALE_NPROTO8_TBL + 1 + SBCDEC_FIXED_EXTRA_BITS)

extern const int32_t sbc_proto_4_40m0[];

extern const int32_t sbc_proto_4_40m1[] ;

extern const int32_t sbc_proto_8_80m0[];

extern const int32_t sbc_proto_8_80m1[];

extern const int32_t synmatrix4[8][4];

extern const int32_t synmatrix8[16][8];

/*
 * Enforce 16 byte alignment for the data, which is supposed to be used
 * with SIMD optimized code.
 */

#define SBC_ALIGN_BITS 4
#define SBC_ALIGN_MASK ((1 << (SBC_ALIGN_BITS)) - 1)
/*
#ifdef __GNUC__
#define SBC_ALIGNED __attribute__((aligned(1 << (SBC_ALIGN_BITS))))
#else
#define SBC_ALIGNED
#endif
*/

/* Uncomment the following line to enable high precision build of SBC encoder */

/* #define SBC_HIGH_PRECISION */

#ifdef SBC_HIGH_PRECISION
#define FIXED_A int64_t /* data type for fixed point accumulator */
#define FIXED_T int32_t /* data type for fixed point constants */
#define SBC_FIXED_EXTRA_BITS 16
#else
#define FIXED_A int32_t /* data type for fixed point accumulator */
#define FIXED_T int16_t /* data type for fixed point constants */
#define SBC_FIXED_EXTRA_BITS 0
#endif

/* A2DP specification: Section 12.8 Tables
 *
 * Original values are premultiplied by 2 for better precision (that is the
 * maximum which is possible without overflows)
 *
 * Note: in each block of 8 numbers sign was changed for elements 2 and 7
 * in order to compensate the same change applied to cos_table_fixed_4
 */
#define SBC_PROTO_FIXED4_SCALE \
	((sizeof(FIXED_T) * CHAR_BIT - 1) - SBC_FIXED_EXTRA_BITS + 1)
#define F_PROTO4(x) (FIXED_A) ((x * 2) * \
	((FIXED_A) 1 << (sizeof(FIXED_T) * CHAR_BIT - 1)) + 0.5)

/*
 * To produce this cosine matrix in Octave:
 *
 * b = zeros(4, 8);
 * for i = 0:3
 * for j = 0:7 b(i+1, j+1) = cos((i + 0.5) * (j - 2) * (pi/4))
 * endfor
 * endfor;
 * printf("%.10f, ", b');
 *
 * Note: in each block of 8 numbers sign was changed for elements 2 and 7
 *
 * Change of sign for element 2 allows to replace constant 1.0 (not
 * representable in Q15 format) with -1.0 (fine with Q15).
 * Changed sign for element 7 allows to have more similar constants
 * and simplify subband filter function code.
 */
#define SBC_COS_TABLE_FIXED4_SCALE \
	((sizeof(FIXED_T) * CHAR_BIT - 1) + SBC_FIXED_EXTRA_BITS)
#define F_COS4(x) (FIXED_A) ((x) * \
	((FIXED_A) 1 << (sizeof(FIXED_T) * CHAR_BIT - 1)) + 0.5)

/* A2DP specification: Section 12.8 Tables
 *
 * Original values are premultiplied by 4 for better precision (that is the
 * maximum which is possible without overflows)
 *
 * Note: in each block of 16 numbers sign was changed for elements 4, 13, 14, 15
 * in order to compensate the same change applied to cos_table_fixed_8
 */
#define SBC_PROTO_FIXED8_SCALE \
	((sizeof(FIXED_T) * CHAR_BIT - 1) - SBC_FIXED_EXTRA_BITS + 1)
#define F_PROTO8(x) (FIXED_A) ((x * 2) * \
	((FIXED_A) 1 << (sizeof(FIXED_T) * CHAR_BIT - 1)) + 0.5)

/*
 * To produce this cosine matrix in Octave:
 *
 * b = zeros(8, 16);
 * for i = 0:7
 * for j = 0:15 b(i+1, j+1) = cos((i + 0.5) * (j - 4) * (pi/8))
 * endfor endfor;
 * printf("%.10f, ", b');
 *
 * Note: in each block of 16 numbers sign was changed for elements 4, 13, 14, 15
 *
 * Change of sign for element 4 allows to replace constant 1.0 (not
 * representable in Q15 format) with -1.0 (fine with Q15).
 * Changed signs for elements 13, 14, 15 allow to have more similar constants
 * and simplify subband filter function code.
 */
#define SBC_COS_TABLE_FIXED8_SCALE \
	((sizeof(FIXED_T) * CHAR_BIT - 1) + SBC_FIXED_EXTRA_BITS)
#define F_COS8(x) (FIXED_A) ((x) * \
	((FIXED_A) 1 << (sizeof(FIXED_T) * CHAR_BIT - 1)) + 0.5)

extern const FIXED_T _sbc_proto_fixed4[40];

extern const FIXED_T cos_table_fixed_4[32];

extern const FIXED_T _sbc_proto_fixed8[80];

extern const FIXED_T cos_table_fixed_8[128];
/*
 * Enforce 16 byte alignment for the data, which is supposed to be used
 * with SIMD optimized code.
 */

#define SBC_ALIGN_BITS 4
#define SBC_ALIGN_MASK ((1 << (SBC_ALIGN_BITS)) - 1)

/*#ifdef __GNUC__
#define SBC_ALIGNED __attribute__((aligned(1 << (SBC_ALIGN_BITS))))
#else
#define SBC_ALIGNED
#endif
*/

/*
 * Constant tables for the use in SIMD optimized analysis filters
 * Each table consists of two parts:
 * 1. reordered "proto" table
 * 2. reordered "cos" table
 *
 * Due to non-symmetrical reordering, separate tables for "even"
 * and "odd" cases are needed
 */

extern const FIXED_T SBC_ALIGNED analysis_consts_fixed4_simd_even[40 + 16];

extern const FIXED_T SBC_ALIGNED analysis_consts_fixed4_simd_odd[40 + 16];


extern const FIXED_T SBC_ALIGNED analysis_consts_fixed8_simd_even[80 + 64];

extern const FIXED_T SBC_ALIGNED analysis_consts_fixed8_simd_odd[80 + 64];

