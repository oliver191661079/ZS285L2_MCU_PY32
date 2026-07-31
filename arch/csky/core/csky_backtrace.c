/****************************************************************************
 * arch/csky/src/cskyv2-l/csky_backtrace.c
 *
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#include <kernel_structs.h>
#include <kallsyms.h>
#include <stack_backtrace.h>
#include "csky_backtrace.h"

inline static int csky_get_insn(uint32_t addr, uint32_t *insn)
{
	if ((*(unsigned short *) addr >> 14) == 0x3) {
		*insn = *(unsigned short *) (addr + 2)
				| (*(unsigned short *) addr) << 16;
		return 4;
	} else {
		*insn = *(unsigned short *) addr;
		return 2;
	}
}

/*
 * Get the index of a function in allsyms.
 */
static int get_start_limit_addr(uint32_t pc, uint32_t *pstart_pc,
				uint32_t *plimit_pc)
{
	unsigned long offs, size;

	if (!kallsyms_lookup_size_offset(pc, &size, &offs))
		return -1;

	*pstart_pc = pc - offs;
	*plimit_pc = *pstart_pc + size;

	return 0;
}

/*
 * Analyze a funtion insn by insn to get the stack information.
 * start_pc: Start pc of the function
 * limit_pc: End pc of the function
 * lr_offset: Store offset of lr storage
 * epc_offset: Store offset of lr storage
 * sp_adjust: Store stack length of the function
 */

static void csky_analyze_prologue(uint32_t start_pc, uint32_t limit_pc,
				  uint32_t *lr_offset, uint32_t *subi_len)
{
	uint32_t addr;
	uint32_t insn, rn;
	int framesize;
	int stacksize;
#define CSKY_NUM_GREGS_v2  32
#define CSKY_NUM_GREGS_v2_SAVED_GREGS   (CSKY_NUM_GREGS_v2+4)
	int register_offsets[CSKY_NUM_GREGS_v2_SAVED_GREGS]; // 32 general regs + 4
	int insn_len;

	int mfcr_regnum;
	int stw_regnum;

	/*
	 * When build programe with -fno-omit-frame-pointer, there must be
	 * mov r8, r0
	 * in prologue, here, we check such insn, once hit, we set IS_FP_SAVED.
	 */

	/* REGISTER_OFFSETS will contain offsets, from the top of the frame
	 * (NOT the frame pointer), for the various saved registers or -1
	 * if the register is not saved. */
	for (rn = 0; rn < CSKY_NUM_GREGS_v2_SAVED_GREGS; rn++) {
		register_offsets[rn] = -1;
	}

	/* Analyze the prologue. Things we determine from analyzing the
	 * prologue include:
	 * the size of the frame
	 * where saved registers are located (and which are saved)
	 * FP used?
	 */

	stacksize = 0;
	insn_len = 2; //instruction is 16bit
	for (addr = start_pc; addr < limit_pc; addr += insn_len) {
		/* Get next insn */
		insn_len = csky_get_insn(addr, &insn);

		if (insn_len == 4) { //if 32bit
			if (V2_32_IS_SUBI0(insn)) { //subi32 sp,sp oimm12
				int offset = V2_32_SUBI_IMM(insn); //got oimm12
				stacksize += offset;
				continue;
			} else if (V2_32_IS_STMx0(insn)) { //stm32 ry-rz,(sp)
				/* Spill register(s) */
				int start_register;
				int reg_count;
				int offset;

				/* BIG WARNING! The CKCore ABI does not restrict functions
				 * to taking only one stack allocation. Therefore, when
				 * we save a register, we record the offset of where it was
				 * saved relative to the current stacksize. This will
				 * then give an offset from the SP upon entry to our
				 * function. Remember, stacksize is NOT constant until
				 * we're done scanning the prologue.
				 */
				start_register = V2_32_STM_VAL_REGNUM(insn);  //ry
				reg_count = V2_32_STM_SIZE(insn);

				for (rn = start_register, offset = 0;
						rn <= start_register + reg_count; rn++, offset += 4)
					register_offsets[rn] = stacksize - offset;

				continue;
			} else if (V2_32_IS_STWx0(insn)) { //stw ry,(sp,disp)
				/* Spill register: see note for IS_STM above. */
				int disp;

				rn = V2_32_ST_VAL_REGNUM(insn);
				disp = V2_32_ST_OFFSET(insn);
				register_offsets[rn] = stacksize - disp;
				continue;
			} else if (V2_32_IS_MOV_FP_SP(insn))
				// Do not forget to skip this insn.
				continue;
			else if (V2_32_IS_MFCR_EPSR(insn)) {
				uint32_t insn2;
				addr += 4;
				mfcr_regnum = insn & 0x1f;
				insn_len = csky_get_insn(addr, &insn2);
				if (insn_len == 2) {
					stw_regnum = (insn2 >> 5) & 0x7;
					if (V2_16_IS_STWx0(insn2) && (mfcr_regnum == stw_regnum)) {
						int offset;

						rn = CSKY_NUM_GREGS_v2; //CSKY_EPSR_REGNUM
						offset = V2_16_STWx0_OFFSET(insn2);
						register_offsets[rn] = stacksize - offset;
						continue;
					}
					break;
				} else { // insn_len == 4
					stw_regnum = (insn2 >> 21) & 0x1f;
					if (V2_32_IS_STWx0(insn2) && (mfcr_regnum == stw_regnum)) {
						int offset;

						rn = CSKY_NUM_GREGS_v2; //CSKY_EPSR_REGNUM
						offset = V2_32_ST_OFFSET(insn2);
						register_offsets[rn] = framesize - offset;
						continue;
					}
					break;
				}
			} else if (V2_32_IS_MFCR_FPSR(insn)) {
				uint32_t insn2;
				addr += 4;
				mfcr_regnum = insn & 0x1f;
				insn_len = csky_get_insn(addr, &insn2);
				if (insn_len == 2) {
					stw_regnum = (insn2 >> 5) & 0x7;
					if (V2_16_IS_STWx0(insn2) && (mfcr_regnum == stw_regnum)) {
						int offset;

						rn = CSKY_NUM_GREGS_v2 + 1; //CSKY_FPSR_REGNUM
						offset = V2_16_STWx0_OFFSET(insn2);
						register_offsets[rn] = stacksize - offset;
						continue;
					}
					break;
				} else { // insn_len == 4
					stw_regnum = (insn2 >> 21) & 0x1f;
					if (V2_32_IS_STWx0(insn2) && (mfcr_regnum == stw_regnum)) {
						int offset;

						rn = CSKY_NUM_GREGS_v2 + 1; //CSKY_FPSR_REGNUM
						offset = V2_32_ST_OFFSET(insn2);
						register_offsets[rn] = framesize - offset;
						continue;
					}
					break;
				}
			} else if (V2_32_IS_MFCR_EPC(insn)) {
				uint32_t insn2;
				addr += 4;
				mfcr_regnum = insn & 0x1f;
				insn_len = csky_get_insn(addr, &insn2);
				if (insn_len == 2) {
					stw_regnum = (insn2 >> 5) & 0x7;
					if (V2_16_IS_STWx0(insn2) && (mfcr_regnum == stw_regnum)) {
						int offset;

						rn = CSKY_NUM_GREGS_v2 + 2; //CSKY_EPC_REGNUM
						offset = V2_16_STWx0_OFFSET(insn2);
						register_offsets[rn] = stacksize - offset;
						continue;
					}
					break;
				} else { // insn_len == 4
					stw_regnum = (insn2 >> 21) & 0x1f;
					if (V2_32_IS_STWx0(insn2) && (mfcr_regnum == stw_regnum)) {
						int offset;

						rn = CSKY_NUM_GREGS_v2 + 2; //CSKY_EPC_REGNUM
						offset = V2_32_ST_OFFSET(insn2);
						register_offsets[rn] = framesize - offset;
						continue;
					}
					break;
				}
			} else if (V2_32_IS_MFCR_FPC(insn)) {
				uint32_t insn2;
				addr += 4;
				mfcr_regnum = insn & 0x1f;
				insn_len = csky_get_insn(addr, &insn2);

				if (insn_len == 2) {
					stw_regnum = (insn2 >> 5) & 0x7;
					if (V2_16_IS_STWx0(insn2) && (mfcr_regnum == stw_regnum)) {
						int offset;

						rn = CSKY_NUM_GREGS_v2 + 3; // CSKY_FPC_REGNUM
						offset = V2_16_STWx0_OFFSET(insn2);
						register_offsets[rn] = stacksize - offset;
						continue;
					}
					break;
				} else { // insn_len == 4
					stw_regnum = (insn2 >> 21) & 0x1f;
					if (V2_32_IS_STWx0(insn2) && (mfcr_regnum == stw_regnum)) {
						int offset;

						rn = CSKY_NUM_GREGS_v2 + 3; //CSKY_FPC_REGNUM
						offset = V2_32_ST_OFFSET(insn2);
						register_offsets[rn] = framesize - offset;
						continue;
					}
					break;
				}
			} else if (V2_32_IS_PUSH(insn)) { //push for 32_bit
				int offset = 0;
				if (V2_32_IS_PUSH_R29(insn)) {
					stacksize += 4;
					register_offsets[29] = stacksize;
					offset += 4;
				}
				if (V2_32_PUSH_LIST2(insn)) {
					int num = V2_32_PUSH_LIST2(insn);
					int tmp = 0;
					stacksize += num * 4;
					offset += num * 4;
					for (rn = 16; rn <= 16 + num - 1; rn++) {
						register_offsets[rn] = stacksize - tmp;
						tmp += 4;
					}
				}
				if (V2_32_IS_PUSH_R15(insn)) {
					stacksize += 4;
					register_offsets[15] = stacksize;
					offset += 4;
				}
				if (V2_32_PUSH_LIST1(insn)) {
					int tmp = 0;
					int num = V2_32_PUSH_LIST2(insn);
					stacksize += num * 4;
					offset += num * 4;
					for (rn = 4; rn <= 4 + num - 1; rn++) {
						register_offsets[rn] = stacksize - tmp;
						tmp += 4;
					}
				}

				framesize = stacksize;
				continue;
			}  //end of push for 32_bit
			else if (V2_32_IS_LRW4(insn) || V2_32_IS_MOVI4(insn)
					|| V2_32_IS_MOVIH4(insn) || V2_32_IS_BMASKI4(insn)) {
				uint32_t adjust = 0;
				int offset = 0;
				uint32_t insn2;

				if (V2_32_IS_LRW4(insn)) {
					int literal_addr = (addr + ((insn & 0xffff) << 2))
							& 0xfffffffc;
					csky_get_insn(literal_addr, &adjust);
				} else if (V2_32_IS_MOVI4(insn))
					adjust = (insn & 0xffff);
				else if (V2_32_IS_MOVIH4(insn))
					adjust = (insn & 0xffff) << 16;
				else
					/* V2_32_IS_BMASKI4 (insn) */
					adjust = (1 << (((insn & 0x3e00000) >> 21) + 1)) - 1;

				/* May have zero or more insns which modify r4 */
				offset = 4;
				insn_len = csky_get_insn(addr + offset, &insn2);
				while (V2_IS_R4_ADJUSTER(insn2)) {
					if (V2_32_IS_ADDI4(insn2)) {
						int imm = (insn2 & 0xfff) + 1;
						adjust += imm;
					} else if (V2_32_IS_SUBI4(insn2)) {
						int imm = (insn2 & 0xfff) + 1;
						adjust -= imm;
					} else if (V2_32_IS_NOR4(insn2))
						adjust = ~adjust;
					else if (V2_32_IS_ROTLI4(insn2)) {
						int imm = ((insn2 >> 21) & 0x1f);
						int temp = adjust >> (32 - imm);
						adjust <<= imm;
						adjust |= temp;
					} else if (V2_32_IS_LISI4(insn2)) {
						int imm = ((insn2 >> 21) & 0x1f);
						adjust <<= imm;
					} else if (V2_32_IS_BSETI4(insn2)) {
						int imm = ((insn2 >> 21) & 0x1f);
						adjust |= (1 << imm);
					} else if (V2_32_IS_BCLRI4(insn2)) {
						int imm = ((insn2 >> 21) & 0x1f);
						adjust &= ~(1 << imm);
					} else if (V2_32_IS_IXH4(insn2))
						adjust *= 3;
					else if (V2_32_IS_IXW4(insn2))
						adjust *= 5;
					else if (V2_16_IS_ADDI4(insn2)) {
						int imm = (insn2 & 0xff) + 1;
						adjust += imm;
					} else if (V2_16_IS_SUBI4(insn2)) {
						int imm = (insn2 & 0xff) + 1;
						adjust -= imm;
					} else if (V2_16_IS_NOR4(insn2))
						adjust = ~adjust;
					else if (V2_16_IS_BSETI4(insn2)) {
						int imm = (insn2 & 0x1f);
						adjust |= (1 << imm);
					} else if (V2_16_IS_BCLRI4(insn2)) {
						int imm = (insn2 & 0x1f);
						adjust &= ~(1 << imm);
					} else if (V2_16_IS_LSLI4(insn2)) {
						int imm = (insn2 & 0x1f);
						adjust <<= imm;
					}

					offset += insn_len;
					insn_len = csky_get_insn(addr + offset, &insn2);
				};

				/*
				 * If the next insn adjusts the stack pointer, we keep everything;
				 * if not, we scrap it and we've found the end of the prologue.
				 */
				if (V2_IS_SUBU4(insn2)) {
					addr += offset;
					stacksize += adjust;
					continue;
				}

				/* None of these instructions are prologue, so don't touch anything. */
				break;
			}
		}   // end of ' if(insn_len == 4)'
		else {
			if (V2_16_IS_SUBI0(insn)) { //subi.sp sp,disp
				int offset = V2_16_SUBI_IMM(insn);
				stacksize += offset; //capacity of creating space in stack
				continue;
			} else if (V2_16_IS_STWx0(insn)) { //stw.16 rz,(sp,disp)
				/* Spill register: see note for IS_STM above. */
				int disp;

				rn = V2_16_ST_VAL_REGNUM(insn);
				disp = V2_16_ST_OFFSET(insn);
				register_offsets[rn] = stacksize - disp;
				continue;
			} else if (V2_16_IS_MOV_FP_SP(insn))
				// We do nothing here except omit this instruction
				continue;
			else if (V2_16_IS_PUSH(insn)) { //push for 16_bit
				int offset = 0;
				if (V2_16_IS_PUSH_R15(insn)) {
					stacksize += 4;
					register_offsets[15] = stacksize;
					offset += 4;
				}
				if (V2_16_PUSH_LIST1(insn)) {
					int num = V2_16_PUSH_LIST1(insn);
					int tmp = 0;
					stacksize += num * 4;
					offset += num * 4;
					for (rn = 4; rn <= 4 + num - 1; rn++) {
						register_offsets[rn] = stacksize - tmp;
						tmp += 4;
					}
				}

				framesize = stacksize;
				continue;
			} // end of push for 16_bit
			else if (V2_16_IS_LRW4(insn) || V2_16_IS_MOVI4(insn)) {
				int adjust = 0;
				int offset = 0;
				uint32_t insn2;

				if (V2_16_IS_LRW4(insn)) {
					offset = ((insn & 0x300) >> 3) | (insn & 0x1f);
					int literal_addr = (addr + (offset << 2)) & 0xfffffffc;
					adjust = *(uint32_t*) literal_addr;
				} else
					// V2_16_IS_MOVI4(insn)
					adjust = (insn & 0xff);

				/* May have zero or more insns which modify r4 */
				offset = 2;
				insn_len = csky_get_insn(addr + offset, &insn2);
				while (V2_IS_R4_ADJUSTER(insn2)) {
					if (V2_32_IS_ADDI4(insn2)) {
						int imm = (insn2 & 0xfff) + 1;
						adjust += imm;
					} else if (V2_32_IS_SUBI4(insn2)) {
						int imm = (insn2 & 0xfff) + 1;
						adjust -= imm;
					} else if (V2_32_IS_NOR4(insn2))
						adjust = ~adjust;
					else if (V2_32_IS_ROTLI4(insn2)) {
						int imm = ((insn2 >> 21) & 0x1f);
						int temp = adjust >> (32 - imm);
						adjust <<= imm;
						adjust |= temp;
					} else if (V2_32_IS_LISI4(insn2)) {
						int imm = ((insn2 >> 21) & 0x1f);
						adjust <<= imm;
					} else if (V2_32_IS_BSETI4(insn2)) {
						int imm = ((insn2 >> 21) & 0x1f);
						adjust |= (1 << imm);
					} else if (V2_32_IS_BCLRI4(insn2)) {
						int imm = ((insn2 >> 21) & 0x1f);
						adjust &= ~(1 << imm);
					} else if (V2_32_IS_IXH4(insn2))
						adjust *= 3;
					else if (V2_32_IS_IXW4(insn2))
						adjust *= 5;
					else if (V2_16_IS_ADDI4(insn2)) {
						int imm = (insn2 & 0xff) + 1;
						adjust += imm;
					} else if (V2_16_IS_SUBI4(insn2)) {
						int imm = (insn2 & 0xff) + 1;
						adjust -= imm;
					} else if (V2_16_IS_NOR4(insn2))
						adjust = ~adjust;
					else if (V2_16_IS_BSETI4(insn2)) {
						int imm = (insn2 & 0x1f);
						adjust |= (1 << imm);
					} else if (V2_16_IS_BCLRI4(insn2)) {
						int imm = (insn2 & 0x1f);
						adjust &= ~(1 << imm);
					} else if (V2_16_IS_LSLI4(insn2)) {
						int imm = (insn2 & 0x1f);
						adjust <<= imm;
					}

					offset += insn_len;
					insn_len = csky_get_insn(addr + offset, &insn2);
				};
				/* If the next insn adjusts the stack pointer, we keep everything;
				 if not, we scrap it and we've found the end of the prologue. */
				if (V2_IS_SUBU4(insn2)) {
					addr += offset;
					stacksize += adjust;
					continue;
				}

				/* None of these instructions are prologue, so don't touch anything. */
				break;
			}
		}

		/* This is not a prologue insn, so stop here. */
		break;

	}
	*subi_len = stacksize;
	*lr_offset = register_offsets[15];
}

void csky_backtrace(uint32_t addr, uint32_t sp, uint32_t bak_lr)
{
	uint32_t start_pc = 0, limit_pc = 0;
	uint32_t lr_offset;
	uint32_t flag_maybe_leaf_f = 1;
	uint32_t subi_len;

	print_ip_sym(addr);

	while(1) {
		if (get_start_limit_addr(addr, &start_pc, &limit_pc)) {
			break;
		}
		csky_analyze_prologue(start_pc, limit_pc,
				    &lr_offset,&subi_len);

		if ((lr_offset != -1 || lr_offset != -1) && (subi_len != 0)) {
			if (lr_offset != -1) {
				addr = *(uint32_t *)(sp + subi_len - lr_offset);
				flag_maybe_leaf_f = 1;
			} else {
				addr = *(uint32_t *)(sp + subi_len - lr_offset);
				flag_maybe_leaf_f = 0;
			}
			sp += subi_len;

			if (addr)
				print_ip_sym(addr);

		} else if (flag_maybe_leaf_f && bak_lr != 0) {
			addr = bak_lr;

			if (addr)
				print_ip_sym(addr);

			flag_maybe_leaf_f = 0;
		} else {
			break;
		}
	}
}