/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <debug/coredump.h>

#define ARCH_HDR_VER			1

struct csky_arch_block {
	struct {
		uint32_t	r0;
		uint32_t	r1;
		uint32_t	r2;
		uint32_t	r3;
		uint32_t	psp;
		uint32_t	ra;
		uint32_t	epc;
		uint32_t    epsr;
		uint32_t	sp;
	} r;
} __packed;

/*
 * This might be too large for stack space if defined
 * inside function. So do it here.
 */
static struct csky_arch_block arch_blk;

void arch_coredump_info_dump(const z_arch_esf_t *esf)
{
	struct coredump_arch_hdr_t hdr = {
		.id = COREDUMP_ARCH_HDR_ID,
		.hdr_version = ARCH_HDR_VER,
		.num_bytes = sizeof(arch_blk),
	};

	/* Nothing to process */
	if (esf == NULL) {
		return;
	}

	(void)memset(&arch_blk, 0, sizeof(arch_blk));

	/*
	 * 17 registers expected by GDB.
	 * Not all are in ESF but the GDB stub
	 * will need to send all 17 as one packet.
	 * The stub will need to send undefined
	 * for registers not presented in coredump.
	 */
	arch_blk.r.r0 = esf->r0;
	arch_blk.r.r1 = esf->r1;
	arch_blk.r.r2 = esf->r2;
	arch_blk.r.r3 = esf->r3;
	arch_blk.r.psp = esf->r14;
	arch_blk.r.epsr = esf->epsr;
	arch_blk.r.ra = esf->r15;
	arch_blk.r.epc = esf->epc;

	arch_blk.r.sp = (uint32_t)esf;

	/* Send for output */
	coredump_buffer_output((uint8_t *)&hdr, sizeof(hdr));
	coredump_buffer_output((uint8_t *)&arch_blk, sizeof(arch_blk));
}

uint16_t arch_coredump_tgt_code_get(void)
{
	return COREDUMP_TGT_CSKY;
}
