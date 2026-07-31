/****************************************************************************
 * /arch/src/cskyv2/cskyv2_backtrace.h
 *
 * Copyright (C) 2016 The YunOS Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ****************************************************************************/
#ifndef CSKY_BACKTRACE_H
#define CSKY_BACKTRACE_H

#include <stdint.h>

/* Instruction macros used for analyzing the prologue */
#define V2P_16_IS_ST(insn)            (((insn & 0xf000) == 0x5000) && ((insn & 0x0c00) != 0x0c00))
                                                /* st.b/h/w, the buttom half 0x0c00 is stm  */
#define V2P_16_ST_SIZE(insn)          (1 << ((insn & 0x0c00) >> 10)) /* size */
#define V2P_16_ST_ADDR_REGNUM(insn)   ((insn & 0x003c) >> 2)         /* rx */
#define V2P_16_ST_OFFSET(insn)        (insn & 0x3)                   /* disp */
#define V2P_16_ST_VAL_REGNUM(insn)    ((insn & 0x03c0) >> 6)         /* ry */
#define V2P_16_IS_STWx0(insn)         ((insn & 0xfc3c) == 0x5800)    /* stw ry, (r0, disp) */

#define V2P_16_IS_STM(insn)           ((insn & 0xfc00) == 0x5c00)
#define V2P_16_STM_ADDR_REGNUM(insn)  V2P_16_ST_ADDR_REGNUM(insn)     /* rx */
#define V2P_16_STM_SIZE(insn)         ((insn & 0x3) + 1)             /* count of registers */

#define V2P_16_IS_LD(insn)            (((insn & 0xf000) == 0x4000) && ((insn & 0x0c00) != 0x0c00))
                                                /* ld.b/h/w, the buttom half 0x0c00 is ldm  */
#define V2P_16_LD_SIZE(insn)          V2P_16_ST_SIZE(insn)            /* size */
#define V2P_16_LD_ADDR_REGNUM(insn)   V2P_16_ST_ADDR_REGNUM(insn)     /* rx */
#define V2P_16_LD_OFFSET(insn)        V2P_16_ST_OFFSET(insn)          /* disp */

#define V2P_16_IS_LDM(insn)           ((insn & 0xfc00) == 0x4c00)
#define V2P_16_LDM_ADDR_REGNUM(insn)  V2P_16_STM_ADDR_REGNUM(insn)    /* rx */
#define V2P_16_LDM_SIZE(insn)         V2P_16_STM_SIZE(insn)           /* count of registers */
#define V2P_16_STM_VAL_REGNUM(insn)   ((insn & 0x03c0) >> 6)         /* ry */
#define V2P_16_IS_STMx0(insn)         ((insn & 0xfc3c) == 0x5c00)    /* stm  ry-rz,(r0) */

#define V2P_32_IS_ST(insn)            ((insn & 0xfc00c000) == 0xdc000000)
                                                                    /* st.b/h/w */
#define V2P_32_ST_SIZE(insn)          (1 << ((insn & 0x3000) >> 12)) /* size: b/h/w */
#define V2P_32_ST_ADDR_REGNUM(insn)   ((insn & 0x001f0000) >> 16)    /* rx */
#define V2P_32_ST_OFFSET(insn)        (insn & 0x00000fff)            /* disp */
#define V2P_32_ST_VAL_REGNUM(insn)    ((insn & 0x03e00000) >> 21)    /* ry */
#define V2P_32_IS_STWx0(insn)         ((insn & 0xfc1ff000) == 0xdc002000)
                                                                    /* stw ry, (r0, disp) */

#define V2P_32_IS_STM(insn)           ((insn & 0xfc00ffe0) == 0xd4001c20)
#define V2P_32_STM_ADDR_REGNUM(insn)  V2P_32_ST_ADDR_REGNUM(insn)     /* rx */
#define V2P_32_STM_SIZE(insn)         ((insn & 0x1f) + 1)            /* count of registers */
#define V2P_32_STM_VAL_REGNUM(insn)   ((insn & 0x03e00000) >> 21)    /* ry */
#define V2P_32_IS_STMx0(insn)         ((insn & 0xfc1fffe0) == 0xd4001c20)
                                                                    /* stm ry-rz,(r0) */

#define V2P_32_IS_STR(insn)           (((insn & 0xfc000000) == 0xd4000000) && !(V2P_32_IS_STM(insn)))
#define V2P_32_STR_X_REGNUM(insn)     V2P_32_ST_ADDR_REGNUM(insn)     /* rx */
#define V2P_32_STR_Y_REGNUM(insn)     ((insn >> 21) & 0x1f)          /* ry */
#define V2P_32_STR_SIZE(insn)         (1 << ((insn & 0x0c00) >> 10)) /* size: b/h/w */
#define V2P_32_STR_OFFSET(insn)       ((insn & 0x000003e0) >> 5)     /* imm (for rx + ry*imm)*/

#define V2P_32_IS_STEX(insn)          ((insn & 0xfc00f000) == 0xdc007000)
#define V2P_32_STEX_ADDR_REGNUM(insn) ((insn & 0xf0000) >> 16)       /* rx */
#define V2P_32_STEX_OFFSET(insn)      (insn & 0x0fff)                /* disp */

#define V2P_32_IS_LD(insn)            ((insn & 0xfc008000) == 0xd8000000)
                                                                    /* ld.b/h/w */
#define V2P_32_LD_SIZE(insn)          V2P_32_ST_SIZE(insn)            /* size */
#define V2P_32_LD_ADDR_REGNUM(insn)   V2P_32_ST_ADDR_REGNUM(insn)     /* rx */
#define V2P_32_LD_OFFSET(insn)        V2P_32_ST_OFFSET(insn)          /* disp */

#define V2P_32_IS_LDM(insn)           ((insn & 0xfc00ffe0) == 0xd0001c20)
#define V2P_32_LDM_ADDR_REGNUM(insn)  V2P_32_STM_ADDR_REGNUM(insn)    /* rx */
#define V2P_32_LDM_SIZE(insn)         V2P_32_STM_SIZE(insn)           /* count of registers */

#define V2P_32_IS_LDR(insn)           (((insn & 0xfc00fe00) == 0xd0000000) && !(V2P_32_IS_LDM(insn))) 
#define V2P_32_LDR_X_REGNUM(insn)     V2P_32_STR_X_REGNUM(insn)       /* rx */
#define V2P_32_LDR_Y_REGNUM(insn)     V2P_32_STR_Y_REGNUM(insn)       /* ry */
#define V2P_32_LDR_SIZE(insn)         V2P_32_STR_SIZE(insn)           /* size: b/h/w */
#define V2P_32_LDR_OFFSET(insn)       V2P_32_STR_OFFSET(insn)         /* imm (for rx + ry*imm) */

#define V2P_32_IS_LDEX(insn)          ((insn & 0xfc00f000) == 0xd8007000)
#define V2P_32_LDEX_ADDR_REGNUM(insn) V2P_32_STEX_ADDR_REGNUM(insn)   /* rx */
#define V2P_32_LDEX_OFFSET(insn)      V2P_32_STEX_OFFSET(insn)        /* disp */

#define V2P_16_IS_SUBI0(insn)         ((insn & 0xffc1) == 0x2001)    /* subi r0, imm */
#define V2P_16_SUBI_IMM(insn)         (((insn & 0x003e) >> 1) + 1)

#define V2P_32_IS_SUBI0(insn)         ((insn & 0xffff0000) == 0xa4000000)
                                                                    /* subi r0, imm */
#define V2P_32_SUBI_IMM(insn)         ((insn & 0xffff) + 1)

#define V2P_16_IS_MOV_FP_SP(insn)     (insn == 0x1e00)     /* mov r8, r0 */
#define V2P_32_IS_MOV_FP_SP(insn)     (insn == 0xc4004828) /* mov r8, r0 */

#define V2_16_IS_ST(insn)            (((insn & 0xe000)==0xa000) && ((insn & 0x1800) != 0x1800)) /* check st16 but except st16 sp  */
#define V2_16_ST_SIZE(insn)          (1 << ((insn & 0x1800) >> 11))              /* size */
#define V2_16_ST_ADDR_REGNUM(insn)   ((insn & 0x700) >> 8)                       /* rx */
#define V2_16_ST_OFFSET(insn)        ((insn & 0x1f) << ((insn & 0x1800) >> 11))  /* disp */
#define V2_16_ST_VAL_REGNUM(insn)    ((insn &0xe0) >> 5)                         /* ry */
#define V2_16_IS_STWx0(insn)         ((insn & 0xf800) == 0xb800)          /* st16.w rz, (sp, disp) */
#define V2_16_STWx0_VAL_REGNUM(insn) V2_16_ST_ADDR_REGNUM(insn)
#define V2_16_STWx0_OFFSET(insn)     ((((insn & 0x700) >> 3) + (insn &0x1f)) <<2) /*disp*/


/* "stm" only exists in 32_bit insn now */
//#define V2_16_IS_STM(insn)           ((insn & 0xfc00) == 0x5c00)
//#define V2_16_STM_ADDR_REGNUM(insn)  V2_16_ST_ADDR_REGNUM(insn)     /* rx */
//#define V2_16_STM_SIZE(insn)         ((insn & 0x3) + 1)             /* count of registers */

#define V2_16_IS_LD(insn)            (((insn & 0xe000)==0x8000) && (insn & 0x1800) != 0x1800)/*check ld16 but except ld16 sp */
#define V2_16_LD_SIZE(insn)          V2_16_ST_SIZE(insn)            /* size */
#define V2_16_LD_ADDR_REGNUM(insn)   V2_16_ST_ADDR_REGNUM(insn)     /* rx */
#define V2_16_LD_OFFSET(insn)        V2_16_ST_OFFSET(insn)          /* disp */
#define V2_16_IS_LDWx0(insn)         ((insn & 0xf800) == 0x9800)    /*ld16.w rz,(sp,disp)*/
#define V2_16_LDWx0_OFFSET(insn)     V2_16_STWx0_OFFSET(insn)       /*disp*/
/* "ldm" only exists in 32_bit insn now */
//#define V2_16_IS_LDM(insn)           ((insn & 0xfc00) == 0x4c00)
//#define V2_16_LDM_ADDR_REGNUM(insn)  V2_16_STM_ADDR_REGNUM(insn)    /* rx */
//#define V2_16_LDM_SIZE(insn)         V2_16_STM_SIZE(insn)           /* count of registers */
//#define V2_16_STM_VAL_REGNUM(insn)   ((insn & 0x03c0) >> 6)         /* ry */
//#define V2_16_IS_STMx0(insn)         ((insn & 0xfc3c) == 0x5c00)    /* stm  ry-rz,(r0) */

#define V2_32_IS_ST(insn)            ((insn & 0xfc00c000) == 0xdc000000)
                                                                          /* st32.b/h/w/d */
#define V2_32_ST_SIZE(insn)          (1 << ((insn & 0x3000) >> 12))       /* size: b/h/w/d */
#define V2_32_ST_ADDR_REGNUM(insn)   ((insn & 0x001f0000) >> 16)          /* rx */
#define V2_32_ST_OFFSET(insn)        ((insn & 0xfff) << ((insn & 0x3000) >> 12))  /* disp */
#define V2_32_ST_VAL_REGNUM(insn)    ((insn & 0x03e00000) >> 21)          /* ry */
#define V2_32_IS_STWx0(insn)         ((insn & 0xfc1ff000) == 0xdc0e2000)  /* stw ry, (sp, disp) */


#define V2_32_IS_STM(insn)           ((insn & 0xfc00ffe0) == 0xd4001c20)  /*stm32 ry-rz, (rx)*/
#define V2_32_STM_ADDR_REGNUM(insn)  V2_32_ST_ADDR_REGNUM(insn)           /* rx */
#define V2_32_STM_SIZE(insn)         ((insn & 0x1f) + 1)                  /* count of registers */
#define V2_32_STM_VAL_REGNUM(insn)   ((insn & 0x03e00000) >> 21)          /* ry */
#define V2_32_IS_STMx0(insn)         ((insn & 0xfc1fffe0) == 0xd40e1c20)  /* stm32 ry-rz,(sp) */

#define V2_32_IS_STR(insn)           (((insn & 0xfc000000) == 0xd4000000) && !(V2_32_IS_STM(insn)))
                                                                    /*str32.b/h/w rz,(rx,ry<<offset)*/
#define V2_32_STR_X_REGNUM(insn)     V2_32_ST_ADDR_REGNUM(insn)     /* rx */
#define V2_32_STR_Y_REGNUM(insn)     ((insn >> 21) & 0x1f)          /* ry */
#define V2_32_STR_SIZE(insn)         (1 << ((insn & 0x0c00) >> 10)) /* size: b/h/w */
#define V2_32_STR_OFFSET(insn)       ((insn & 0x000003e0) >> 5)     /* imm (for rx + ry*imm)*/

#define V2_32_IS_STEX(insn)          ((insn & 0xfc00f000) == 0xdc007000) /*stex32.w rz,(rx,disp)*/
#define V2_32_STEX_ADDR_REGNUM(insn) ((insn & 0x1f0000) >> 16)           /* rx */
#define V2_32_STEX_OFFSET(insn)      ((insn & 0x0fff) << 2)              /* disp */

#define V2_32_IS_LD(insn)            ((insn & 0xfc00c000) == 0xd8000000) /* ld.b/h/w */
#define V2_32_LD_SIZE(insn)          V2_32_ST_SIZE(insn)                 /* size */
#define V2_32_LD_ADDR_REGNUM(insn)   V2_32_ST_ADDR_REGNUM(insn)          /* rx */
#define V2_32_LD_OFFSET(insn)        V2_32_ST_OFFSET(insn)               /* disp */

#define V2_32_IS_LDM(insn)           ((insn & 0xfc00ffe0) == 0xd0001c20)
#define V2_32_LDM_ADDR_REGNUM(insn)  V2_32_STM_ADDR_REGNUM(insn)    /* rx */
#define V2_32_LDM_SIZE(insn)         V2_32_STM_SIZE(insn)           /* count of registers */

#define V2_32_IS_LDR(insn)           (((insn & 0xfc00fe00) == 0xd0000000) && !(V2_32_IS_LDM(insn)))
                                                                    /*ldr32.b/h/w rz,(rx,ry<<offset)*/
#define V2_32_LDR_X_REGNUM(insn)     V2_32_STR_X_REGNUM(insn)       /* rx */
#define V2_32_LDR_Y_REGNUM(insn)     V2_32_STR_Y_REGNUM(insn)       /* ry */
#define V2_32_LDR_SIZE(insn)         V2_32_STR_SIZE(insn)           /* size: b/h/w */
#define V2_32_LDR_OFFSET(insn)       V2_32_STR_OFFSET(insn)         /* imm (for rx + ry*imm) */

#define V2_32_IS_LDEX(insn)          ((insn & 0xfc00f000) == 0xd8007000)
#define V2_32_LDEX_ADDR_REGNUM(insn) V2_32_STEX_ADDR_REGNUM(insn)   /* rx */
#define V2_32_LDEX_OFFSET(insn)      V2_32_STEX_OFFSET(insn)        /* disp */

#define V2_16_IS_SUBI0(insn)         ((insn & 0xfce0) == 0x1420)    /* subi.sp sp, disp*/
#define V2_16_SUBI_IMM(insn)         ((((insn & 0x300) >> 3) + (insn & 0x1f)) << 2) /*disp*/

#define V2_32_IS_SUBI0(insn)         ((insn & 0xfffff000) == 0xe5ce1000)    /*subi32 sp,sp,oimm12*/
#define V2_32_SUBI_IMM(insn)         ((insn & 0xfff) + 1)                   /*oimm12*/

/* for new instrctions in V2: push */
//push16
#define V2_16_IS_PUSH(insn)          ((insn & 0xffe0) == 0x14c0)
#define V2_16_IS_PUSH_R15(insn)      ((insn & 0x10) == 0x10)
#define V2_16_PUSH_LIST1(insn)       ( insn & 0xf)                          // r4 - r11

// pop16
#define V2_16_IS_POP(insn)           ((insn & 0xffe0) == 0x1480)
#define V2_16_IS_POP_R15(insn)       V2_16_IS_PUSH_R15(insn)
#define V2_16_POP_LIST1(insn)        V2_16_PUSH_LIST1(insn)                 // r4 - r11

//push32
#define V2_32_IS_PUSH(insn)          ((insn & 0xfffffe00) == 0xebe00000)
#define V2_32_IS_PUSH_R29(insn)      ((insn & 0x100) == 0x100)
#define V2_32_IS_PUSH_R15(insn)      ((insn & 0x10) == 0x10)
#define V2_32_PUSH_LIST1(insn)       ( insn & 0xf)         // r4 - r11
#define V2_32_PUSH_LIST2(insn)       ((insn & 0xe0) >> 5)  // r16 - r17

//pop32
#define V2_32_IS_POP(insn)           ((insn & 0xfffffe00) == 0xebc00000)
#define V2_32_IS_POP_R29(insn)      V2_32_IS_PUSH_R29(insn)
#define V2_32_IS_POP_R15(insn)      V2_32_IS_PUSH_R15(insn)
#define V2_32_POP_LIST1(insn)       V2_32_PUSH_LIST1(insn)                // r4 - r11
#define V2_32_POP_LIST2(insn)       V2_32_PUSH_LIST2(insn)                // r16 - r17

//adjust sp by r4(l0)
#define V2_16_IS_LRW4(x)    (((x) & 0xfce0) == 0x1080)        /* lrw r4,literal   */
#define V2_16_IS_MOVI4(x)   (((x) & 0xff00) == 0x3400)        /* movi r4,imm8     */
// insn:bgeni,bmaski only exist in 32_insn now
//#define V2_16_IS_BGENI4(x)  (((x) & 0xfe0f) == 0x3201)      /* bgeni r4,imm5    */
//#define V2_16_IS_BMASKI4(x) (((x) & 0xfe0f) == 0x2C01)      /* bmaski r4,imm5   */
#define V2_16_IS_ADDI4(x)   (((x) & 0xff00) == 0x2400)        /* addi r4,oimm8    */
#define V2_16_IS_SUBI4(x)   (((x) & 0xff00) == 0x2c00)        /* subi r4,oimm8    */
// insn:rsubi not exist in v2_insn
#define V2_16_IS_NOR4(x)    ((x)            == 0x6d12)        /* nor16 r4,r4      */
//insn:rotli not exist in v2_16_insn
#define V2_16_IS_LSLI4(x)   (((x) & 0xffe0) == 0x4480)        /* lsli r4,r4,imm5  */
#define V2_16_IS_BSETI4(x)  (((x) & 0xffe0) == 0x3ca0)        /* bseti r4,imm5    */
#define V2_16_IS_BCLRI4(x)  (((x) & 0xffe0) == 0x3c80)        /* bclri r4,imm5    */
// insn:ixh,ixw only exist in 32_insn now
#define V2_16_IS_SUBU4(x)   ((x)            == 0x6392)        /* subu sp,r4       */

#define V2_16_IS_R4_ADJUSTER(x)   (V2_16_IS_ADDI4(x) || V2_16_IS_SUBI4(x) \
            || V2_16_IS_BSETI4(x) || V2_16_IS_BCLRI4(x) || V2_16_IS_NOR4(x) || V2_16_IS_LSLI4(x))

#define V2_32_IS_LRW4(x)    (((x) & 0xffff0000) == 0xea840000)        /* lrw r4,literal   */
#define V2_32_IS_MOVI4(x)   (((x) & 0xffff0000) == 0xea040000)        /* movi r4,imm16    */
#define V2_32_IS_MOVIH4(x)  (((x) & 0xffff0000) == 0xea240000)        /* movih r4,imm16   */
#define V2_32_IS_BMASKI4(x) (((x) & 0xfc1fffff) == 0xc4005024)        /* bmaski r4,oimm5  */
#define V2_32_IS_ADDI4(x)   (((x) & 0xfffff000) == 0xe4840000)        /* addi r4,r4,oimm12*/
#define V2_32_IS_SUBI4(x)   (((x) & 0xfffff000) == 0xe4810000)        /* subi r4,r4,oimm12*/
// insn:rsubi not exist in v2_insn
#define V2_32_IS_NOR4(x)    ((x)                == 0xc4842484)        /* nor32 r4,r4,r4   */
#define V2_32_IS_ROTLI4(x)  (((x) & 0xfc1fffff) == 0xc4044904)        /* rotli r4,r4,imm5 */
#define V2_32_IS_LISI4(x)   (((x) & 0xfc1fffff) == 0xc4044824)        /* lsli r4,r4,imm5  */
#define V2_32_IS_BSETI4(x)  (((x) & 0xfc1fffff) == 0xc4042844)        /*bseti32 r4,r4,imm5*/
#define V2_32_IS_BCLRI4(x)  (((x) & 0xfc1fffff) == 0xc4042824)        /*bclri32 r4,r4,imm5*/
#define V2_32_IS_IXH4(x)    ((x)                == 0xc4840824)        /* ixh r4,r4,r4     */
#define V2_32_IS_IXW4(x)    ((x)                == 0xc4840844)        /* ixw r4,r4,r4     */
#define V2_32_IS_SUBU4(x)   ((x)                == 0xc48e008e)        /* subu32 sp,sp,r4  */

#define V2_32_IS_R4_ADJUSTER(x)   (V2_32_IS_ADDI4(x) || V2_32_IS_SUBI4(x) \
           || V2_32_IS_ROTLI4(x)  || V2_32_IS_IXH4(x) || V2_32_IS_IXW4(x) \
        || V2_32_IS_NOR4(x)  || V2_32_IS_BSETI4(x) || V2_32_IS_BCLRI4(x) || V2_32_IS_LISI4(x))

#define V2_IS_R4_ADJUSTER(x)  ( V2_32_IS_R4_ADJUSTER(x) || V2_16_IS_R4_ADJUSTER(x) )
#define V2_IS_SUBU4(x)  ( V2_32_IS_SUBU4(x) || V2_16_IS_SUBU4(x) )

// insn:mfcr only exist in v2_32
#define V2_32_IS_MFCR_EPSR(insn)    ((insn & 0xffffffe0) == 0xc0026020)   /* mfcr rz, epsr*/
#define V2_32_IS_MFCR_FPSR(insn)    ((insn & 0xffffffe0) == 0xc0036020)   /* mfcr rz, fpsr*/
#define V2_32_IS_MFCR_EPC(insn)     ((insn & 0xffffffe0) == 0xc0046020)   /* mfcr rz, epc*/
#define V2_32_IS_MFCR_FPC(insn)     ((insn & 0xffffffe0) == 0xc0056020)   /* mfcr rz, fpc*/

// insn:rte,rfi only exit in v2_32
#define V2_32_IS_RTE(insn)    (insn == 0xc0004020)
#define V2_32_IS_RFI(insn)    (insn == 0xc0004420)
#define V2_32_IS_JMP(insn)    ((insn & 0xffe0ffff) == 0xe8c00000)
#define V2_16_IS_JMP(insn)    ((insn & 0xffc3) == 0x7800)
#define V2_32_IS_JMPI(insn)   ((insn & 0xffff0000) == 0xeac00000)
#define V2_32_IS_JMPIX(insn)  ((insn & 0xffe0fffc) == 0xe9e00000)
#define V2_16_IS_JMPIX(insn)  ((insn & 0xf8fc) == 0x38e0)

#define V2_16_IS_BR(insn)     ((insn & 0xfc00) == 0x0400)
#define V2_32_IS_BR(insn)     ((insn & 0xffff0000) == 0xe8000000)
#define V2_16_IS_MOV_FP_SP(insn)     (insn == 0x6e3b)     /* mov r8, r14 */
#define V2_32_IS_MOV_FP_SP(insn)     (insn == 0xc40e4828) /* mov r8, r14 */

void dump_backtrace(uint32_t *regs);
void csky_backtrace(uint32_t addr, uint32_t sp, uint32_t bak_lr);

#endif /* backtrace.h */

