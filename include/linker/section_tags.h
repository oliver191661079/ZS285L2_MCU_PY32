/* Macros for tagging symbols and putting them in the correct sections. */

/*
 * Copyright (c) 2013-2014, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _section_tags__h_
#define _section_tags__h_

#include <toolchain.h>

#if !defined(_ASMLANGUAGE)

#define __noinit		__in_section_unique(NOINIT)
#define __irq_vector_table	_GENERIC_SECTION(IRQ_VECTOR_TABLE)
#define __sw_isr_table		_GENERIC_SECTION(SW_ISR_TABLE)

#if defined(CONFIG_ARM)
#define __kinetis_flash_config_section __in_section_unique(KINETIS_FLASH_CONFIG)
#define __ti_ccfg_section _GENERIC_SECTION(TI_CCFG)
#endif /* CONFIG_ARM */

#define __wifi		__in_section_unique(wifi)

#ifdef CONFIG_XIP
#define __ramfunc	__in_section_unique(ramfunc) __attribute__((noinline))
#else
#define __ramfunc
#endif

#ifdef CONFIG_XIP
#define __coredata	__in_section_unique(coredata)
#else
#define __coredata
#endif


/* the overlay sections for bluetooth subsystem */
#ifdef CONFIG_XIP
#define __btdrv_ovl_data	__in_section_unique(btdrv_ovl_data)
#define __btdrv_ovl_bss		_NODATA_SECTION(.btdrv_ovl_bss)
#define __btdrv_ovl_text	__in_section_unique(btdrv_ovl_text) __attribute__((noinline))
#else
#define __btdrv_ovl_data
#define __btdrv_ovl_bss
#define __btdrv_ovl_text
#endif

/* the overlay sections for bluetooth subsystem */
#ifdef CONFIG_XIP
#define __scache_ovl_bss	 _NODATA_SECTION(.scache_ovl_bss)
#define __amrpcm_ovl_bss     _NODATA_SECTION(.amrpcm_ovl_bss)
#define __opuspcm_ovl_bss    _NODATA_SECTION(.opuspcm_ovl_bss)
#define __pcm_ovl_bss        _NODATA_SECTION(.pcm_ovl_bss)
#define __sinwav_ovl_bss     _NODATA_SECTION(.sinwav_ovl_bss)
#define __pcm_converter_bss	 _NODATA_SECTION(.pcm_converter_bss)
#define __speexpcm_ovl_bss    _NODATA_SECTION(.speexpcm_ovl_bss)
#define __duer_mem_pool_ovl_bss    _NODATA_SECTION(.duer_mem_pool_ovl_bss)
#define __duerpcm_ovl_bss    _NODATA_SECTION(.duerpcm_ovl_bss)
#define __ifly_ovl_bss    _NODATA_SECTION(.ifly_ovl_bss)
#define __voice_wake_up_ovl_bss    _NODATA_SECTION(.voice_wake_up_ovl_bss)
#else
#define __scache_ovl_bss
#define __amrpcm_ovl_bss
#define __opuspcm_ovl_bss
#define __pcm_ovl_bss
#define __sinwav_ovl_bss
#define __pcm_converter_bss
#define __speexpcm_ovl_bss
#define __duer_mem_pool_ovl_bss
#define __duerpcm_ovl_bss
#define __ifly_ovl_bss
#define __voice_wake_up_ovl_bss
#endif
#endif /* !_ASMLANGUAGE */

#define __buildinfo	__in_section_unique(buildinfo)

#define __keymap    __in_section_unique(keymap)

#define __stack_data __noinit  __aligned(STACK_ALIGN)

#define __act_bss   __in_section_unique(actbss)

#define __subwoofer_bss __in_section_unique(subwooferbss)

#define __decoder_bss  __in_section_unique(decoderbss)

#define __stack_sevice_noinit __in_section_unique(stackservicenoinit)

#define __parser_bss   __in_section_unique(parserbss)

#define __decsbc_bss   __in_section_unique(decsbcbss)
#define __ecnsbc_bss   __in_section_unique(encsbcbss)

#define __trace_printbuf __in_section_unique(traceprintbuf)
#define __trace_tempbuf	__in_section_unique(tracetempbuf)
#define __trace_hexbuf	__in_section_unique(tracehexbuf)

#define __btc_host_bss  __in_section_unique(BTCON_TO_HOST_BUF)

#define __stack_noinit __in_section_unique(stacknoinit)

#define __psram_bss   __in_section_unique(psrambss)

#define __lcmusic_bss __in_section_unique(lcmusicbss)

#define __lcmusic_name_bss __in_section_unique(lcmusicname)
#define __lcmusic_full_name_bss __in_section_unique(lcmusicfullname)
#define __fs_bss __in_section_unique(fs_bss)

#define __csb_bss   __in_section_unique(csbbss)
#define __nav_bss   __in_section_unique(navbss)

#define __ota_bss   __in_section_unique(otabss) __aligned(4)

#endif /* _section_tags__h_ */
