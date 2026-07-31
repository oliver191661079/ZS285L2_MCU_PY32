/* linker.ld - Linker command/script file */

/*
 * Copyright (c) 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define _LINKER
#define _ASMLANGUAGE

#include <autoconf.h>
#include <linker/sections.h>
#include <generated_dts_board.h>

#include <linker/linker-defs.h>
#include <linker/linker-tool.h>

#ifdef CONFIG_SECTION_OVERLAY
#include <section_overlay.h>
#endif

IBANK_SIZE = 2K;

/* physical address of RAM */
#ifdef CONFIG_XIP
	#define ROMABLE_REGION FLASH
	#define RAMABLE_REGION SRAM
#else
	#define ROMABLE_REGION SRAM
	#define RAMABLE_REGION SRAM
#endif

#ifdef CONFIG_SOC_PSRAM_DCACHE
	#define BTCON_MEMPOOL_REGION SRAM
#else
	#define BTCON_MEMPOOL_REGION ram_psram_dcache
#endif

#if defined(CONFIG_XIP)
	#define _DATA_IN_ROM __data_rom_start
#else
	#define _DATA_IN_ROM
#endif

#if !defined(SKIP_TO_KINETIS_FLASH_CONFIG)
	#define SKIP_TO_KINETIS_FLASH_CONFIG
#endif

#define ROM_ADDR (CONFIG_FLASH_BASE_ADDRESS + CONFIG_FLASH_LOAD_OFFSET)
#if CONFIG_FLASH_LOAD_SIZE > 0
	#define ROM_SIZE CONFIG_FLASH_LOAD_SIZE
#else
	#define ROM_SIZE (CONFIG_FLASH_SIZE*1K - CONFIG_FLASH_LOAD_OFFSET)
#endif

#if defined(CONFIG_XIP)
	#if defined(CONFIG_IS_BOOTLOADER)
		#define RAM_SIZE (CONFIG_BOOTLOADER_SRAM_SIZE * 1K)
		#define RAM_END (CONFIG_SRAM_BASE_ADDRESS + (CONFIG_SRAM_SIZE * 1K))
		#define RAM_ADDR (RAM_END - RAM_SIZE))
	#else
		#define RAM_SIZE (CONFIG_SRAM_SIZE * 1K)
		#define RAM_END (CONFIG_SRAM_BASE_ADDRESS + (CONFIG_SRAM_SIZE * 1K))
		#define RAM_ADDR (CONFIG_SRAM_BASE_ADDRESS + 256K)

		#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
			#define PRINT_BUFFER_ADDR (RAM_END - CONFIG_TRACE_BUFFER_SIZE)
		#else
			#define MPOOL_SIZE 14K
			#define MPOOL_ADDR (RAM_END - MPOOL_SIZE)
			#define PRINT_BUFFER_ADDR (MPOOL_ADDR - CONFIG_TRACE_BUFFER_SIZE)
		#endif

		#define ESD_DATA_SIZE 0x80
		#define ESD_ADDR (PRINT_BUFFER_ADDR - ESD_DATA_SIZE)

		#define BOOTINFO_SIZE 0x10
		#define BOOTINFO_ADDR (ESD_ADDR - BOOTINFO_SIZE)

		#define RAMFUNC_SIZE 0x3200
		#define RAMFUNC_ADDR (BOOTINFO_ADDR - RAMFUNC_SIZE)

		#define DATA_RAM_SIZE (RAMFUNC_ADDR - RAM_ADDR)
	#endif
#else
	#define RAM_SIZE (CONFIG_SRAM_SIZE * 1K)
	#define RAM_ADDR CONFIG_SRAM_BASE_ADDRESS
#endif

#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
	#define PSRAM_SIZE CONFIG_SOC_MAPPING_PSRAM_SIZE
	#define PSRAM_ADDR CONFIG_SOC_MAPPING_PSRAM_ADDR
#endif

MEMORY
{
	FLASH                 (rx) : ORIGIN = ROM_ADDR, LENGTH = ROM_SIZE

	/* PCMBUF0&1&2&3, 16K*/
	ram_usb_ms_buf        (wx) : ORIGIN = 0x00030000, LENGTH = 0x4000

	/* PCMBUF4&5&6 + URAM0&1, 8K*/
	ram_parser_buf        (wx) : ORIGIN = 0x00034000, LENGTH = 0x2000
	ram_fileselector_buf  (wx) : ORIGIN = 0x00034000, LENGTH = 0x2000

	/* ASRC + SD, 8K */
	ram_asrc0_buf         (wx) : ORIGIN = 0x00036000, LENGTH = 0xE00
	ram_sd0_buf           (wx) : ORIGIN = 0x00036E00, LENGTH = 0x200
	ram_asrc1_buf         (wx) : ORIGIN = 0x00037000, LENGTH = 0x1000

	/* DSP, 64K+128K */
#ifdef CONFIG_AURACAST
	ram_dsp1              (wx) : ORIGIN = 0x00040000, LENGTH = 0xD600
	ram_dsp_free          (wx) : ORIGIN = 0x0004D600, LENGTH = 0x1200
#else
	ram_dsp1              (wx) : ORIGIN = 0x00040000, LENGTH = 0x9000
	ram_dsp_free          (wx) : ORIGIN = 0x00049000, LENGTH = 0x5800
#endif
	ram_bt                (wx) : ORIGIN = 0x0004E800, LENGTH = 0x10400
	ram_bt_bss            (wx) : ORIGIN = 0x0005ec00, LENGTH = 0x1400

	ram_dsp               (wx) : ORIGIN = 0x00060000, LENGTH = 128K

#ifndef CONFIG_MUSIC_EFFECT
        ram_dsp_dae           (wx) : ORIGIN = 0x00060200, LENGTH = 0x8E00
#endif

	SRAM                  (wx) : ORIGIN = RAM_ADDR, LENGTH = DATA_RAM_SIZE

	ram_func              (wx) : ORIGIN = RAMFUNC_ADDR, LENGTH = RAMFUNC_SIZE
	ram_bootinfo          (wx) : ORIGIN = BOOTINFO_ADDR, LENGTH = BOOTINFO_SIZE
	ram_esd_data          (wx) : ORIGIN = ESD_ADDR, LENGTH = ESD_DATA_SIZE
	ram_trace_printbuf    (wx) : ORIGIN = PRINT_BUFFER_ADDR, LENGTH = CONFIG_TRACE_BUFFER_SIZE

#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
	ram_mpool             (wx) : ORIGIN = PSRAM_ADDR, LENGTH = 16K
	PSRAM                 (wx) : ORIGIN = (PSRAM_ADDR + 16K), LENGTH = (PSRAM_SIZE - 16K)
#else
	ram_mpool             (wx) : ORIGIN = MPOOL_ADDR, LENGTH = MPOOL_SIZE
#endif

#ifndef CONFIG_SOC_PSRAM_DCACHE
	/* 32KB psram dcache */
	ram_psram_dcache      (wx) : ORIGIN = 0x68000000, LENGTH = 0x8000
#endif

	/* Used by and documented in include/linker/intlist.ld */
	IDT_LIST              (wx) : ORIGIN = 0xFFFFF7FF, LENGTH = 2K
}

SECTIONS
{
	GROUP_START(ROMABLE_REGION)

	_image_rom_start = ROM_ADDR;

	SECTION_PROLOGUE(_TEXT_SECTION_NAME,,)
	{
		/* flash image header */
		. = 0x0;
		KEEP(*(.img_header))
		KEEP(*(SORT_BY_NAME(".buildinfo.*")))

		_vector_start = .;

		. = CONFIG_TEXT_SECTION_OFFSET;
		KEEP(*(.exp_table))
		KEEP(*(".exp_table.*"))

		KEEP(*(IRQ_VECTOR_TABLE))

		KEEP(*(.openocd_dbg))
		KEEP(*(".openocd_dbg.*"))

		/* Kinetis has to write 16 bytes at 0x400 */
		SKIP_TO_KINETIS_FLASH_CONFIG
		KEEP(*(.kinetis_flash_config))
		KEEP(*(".kinetis_flash_config.*"))

#if defined(CONFIG_GEN_SW_ISR_TABLE) && !defined(CONFIG_IRQ_STAT)
		KEEP(*(SW_ISR_TABLE))
#endif
		_vector_end = .;

		_image_text_start = .;
		*(.text)
		*(".text.*")
		*(.gnu.linkonce.t.*)
	} GROUP_LINK_IN(ROMABLE_REGION)

	_image_text_end = .;

#include <linker/common-rom.ld>

	SECTION_PROLOGUE(_RODATA_SECTION_NAME,,)
	{
		*(.rodata)
		*(".rodata.*")
#ifdef CONFIG_SOC_SERIES_ANDESC_P2
		KEEP(*(.rom_import_api_table))
		KEEP(*(.rom_fixable_api_table))
#endif
		*(.gnu.linkonce.r.*)

#ifdef CONFIG_DSP
		*(.dsp_code)
#endif

#ifdef CONFIG_ACTIONS_TEST
		. = ALIGN(4);
		__test_case_start = .;
		KEEP(*(.test_case_info))
		__test_case_end = .;
#endif

		. = ALIGN(4);
		__keymap_start = .;
		KEEP(*(SORT_BY_NAME(".keymap.*")))
		__keymap_end = .;

		. = ALIGN(4);
		crash_dump_start = .;
		KEEP(*(SORT(.crash_dump_[_A-Z0-9]*)))
		crash_dump_end = .;

		. = ALIGN(4);
		effect_info_start = .;
		KEEP(*(.audio_effect_info))
		effect_info_end = .;

		. = ALIGN(4);
		media_ctrl_start = .;
		KEEP(*(.media_info))
		media_ctrl_end = .;

		__app_entry_table = .;
		KEEP(*(.app_entry))
		__app_entry_end = .;

		__service_entry_table = .;
		KEEP(*(.service_entry))
		__service_entry_end = .;

		__stream_entry_table = .;
		KEEP(*(.stream_entry))
		__stream_entry_end = .;

		__iterator_entry_table = .;
		KEEP(*(.iterator_entry))
		__iterator_entry_end = .;

		. = ALIGN(4);
		__desktop_plugin_entry_table = .;
		KEEP(*(.desktop_plugin_entry))
		__desktop_plugin_entry_end = .;

		a2dp_codec_start = .;
		KEEP(*(.a2dp_codec_info))
		a2dp_codec_end = .;

		__log_const_start = .;
		KEEP(*(SORT(.log_const_*)));
		__log_const_end = .;

#ifdef CONFIG_SECTION_OVERLAY
		. = ALIGN(4);
		__overlay_table = .;
		LONG(OVERLAY_TABLE_MAGIC)
		/* overlay items count */
		LONG(7)

		/* for lib_k1_wav_p.a */
		LONG(OVERLAY_ID_LIBAPWAV)
		LONG(0);
		LONG(0);
		LONG(0);
		LONG(ABSOLUTE(ADDR(.overlay.data.apwav)));
		LONG(SIZEOF(.overlay.data.apwav));
		LONG(LOADADDR(.overlay.data.apwav));
		LONG(ABSOLUTE(ADDR(.overlay.bss.apwav)));
		LONG(SIZEOF(.overlay.bss.apwav));

		/* for lib_k1_mp3_p.a */
		LONG(OVERLAY_ID_LIBAPMP3)
		LONG(0);
		LONG(0);
		LONG(0);
		LONG(ABSOLUTE(ADDR(.overlay.data.apmp3)));
		LONG(SIZEOF(.overlay.data.apmp3));
		LONG(LOADADDR(.overlay.data.apmp3));
		LONG(ABSOLUTE(ADDR(.overlay.bss.apmp3)));
		LONG(SIZEOF(.overlay.bss.apmp3));

		/* for lib_k1_ape_p.a */
		LONG(OVERLAY_ID_LIBAPAPE)
		LONG(0);
		LONG(0);
		LONG(0);
		LONG(ABSOLUTE(ADDR(.overlay.data.apape)));
		LONG(SIZEOF(.overlay.data.apape));
		LONG(LOADADDR(.overlay.data.apape));
		LONG(ABSOLUTE(ADDR(.overlay.bss.apape)));
		LONG(SIZEOF(.overlay.bss.apape));

		/* for lib_k1_wma_p.a */
		LONG(OVERLAY_ID_LIBAPWMA)
		LONG(0);
		LONG(0);
		LONG(0);
		LONG(ABSOLUTE(ADDR(.overlay.data.apwma)));
		LONG(SIZEOF(.overlay.data.apwma));
		LONG(LOADADDR(.overlay.data.apwma));
		LONG(ABSOLUTE(ADDR(.overlay.bss.apwma)));
		LONG(SIZEOF(.overlay.bss.apwma));

		/* for lib_k1_fla_p.a */
		LONG(OVERLAY_ID_LIBAPFLAC)
		LONG(0);
		LONG(0);
		LONG(0);
		LONG(ABSOLUTE(ADDR(.overlay.data.apfla)));
		LONG(SIZEOF(.overlay.data.apfla));
		LONG(LOADADDR(.overlay.data.apfla));
		LONG(ABSOLUTE(ADDR(.overlay.bss.apfla)));
		LONG(SIZEOF(.overlay.bss.apfla));

		/* for lib_k1_a13_p.a */
		LONG(OVERLAY_ID_LIBAPM4A)
		LONG(0);
		LONG(0);
		LONG(0);
		LONG(ABSOLUTE(ADDR(.overlay.data.apaac)));
		LONG(SIZEOF(.overlay.data.apaac));
		LONG(LOADADDR(.overlay.data.apaac));
		LONG(ABSOLUTE(ADDR(.overlay.bss.apaac)));
		LONG(SIZEOF(.overlay.bss.apaac));

		/* for lib_k1_act_d.a */
		LONG(OVERLAY_ID_LIBADACT);
		LONG(0);
		LONG(0);
		LONG(0);
		LONG(ABSOLUTE(ADDR(_ACTDATA_SECTION_NAME)));
		LONG(SIZEOF(_ACTDATA_SECTION_NAME));
		LONG(LOADADDR(_ACTDATA_SECTION_NAME));
		LONG(ABSOLUTE(ADDR(_ACTBSS_SECTION_NAME)));
		LONG(SIZEOF(_ACTBSS_SECTION_NAME));
		. = ALIGN(4);
#endif

#ifdef CONFIG_CUSTOM_RODATA_LD
		/* Located in project source directory */
		#include <custom-rodata.ld>
#endif
		/*
			* For XIP images, in order to avoid the situation when __data_rom_start
			* is 32-bit aligned, but the actual data is placed right after rodata
			* section, which may not end exactly at 32-bit border, pad rodata
			* section, so __data_rom_start points at data and it is 32-bit aligned.
			*
			* On non-XIP images this may enlarge image size up to 3 bytes. This
			* generally is not an issue, since modern ROM and FLASH memory is
			* usually 4k aligned.
			*/
		. = ALIGN(4);
	} GROUP_LINK_IN(ROMABLE_REGION)

	_image_rom_end = .;
	__data_rom_start = .;    /* XIP imaged DATA ROM start addr */

	__btcon_ramfuc_rom_start = LOADADDR(BTCON_ROM);

	_image_text_ramfunc_rom_start = LOADADDR(RAM_FUNCTION);

	GROUP_END(ROMABLE_REGION)

	GROUP_START(RAMABLE_REGION)

	SECTION_DATA_PROLOGUE(_DATA_SECTION_NAME,,)
	{
		_image_ram_start = .;
		__data_ram_start = .;

#ifdef CONFIG_SECTION_OVERLAY
		*(EXCLUDE_FILE (*libdecencsbc.a:* *libdecsbc.a:* *libdeca13.a:* *libdecape.a:* *libdecflac.a:* *libdecamr.a:* *libdecwav.a:* *libdecmp3.a:* *libdecw13.a:* *libdecfla.a:* *lib_k1_act_d.a:* *libencamr.a:* *libwav.a:* *lib_k1_wav_p.a:* *lib_k1_w13_p.a:* *lib_k1_ape_p.a:* *lib_k1_mp3_p.a:* *lib_k1_fla_p.a:* *libkshow.a:* *libbtdrv.a:* *libencpp.a:* *libencopus.a:* *libencspeex.a:* *libdnoise.a:* *libresample.a:*) .data)
		*(EXCLUDE_FILE (*libdecencsbc.a:* *libdecsbc.a:* *libdeca13.a:* *libdecape.a:* *libdecflac.a:* *libdecamr.a:* *libdecwav.a:* *libdecmp3.a:* *libdecw13.a:* *libdecfla.a:* *lib_k1_act_d.a:* *libencamr.a:* *libwav.a:* *lib_k1_wav_p.a:* *lib_k1_w13_p.a:* *lib_k1_ape_p.a:* *lib_k1_mp3_p.a:* *lib_k1_fla_p.a:* *libkshow.a:* *libbtdrv.a:* *libencpp.a:* *libencopus.a:* *libencspeex.a:* *libdnoise.a:* *libresample.a:*) .data.*)
#ifdef CONFIG_LCMUSIC_PUSH_BTSTREAM
		KEEP(*libdecencsbc.a:*(.data .data.*))
#endif
#else
		*(.data)
		*(.data.*)
#endif

#if defined(CONFIG_GEN_SW_ISR_TABLE) && defined(CONFIG_IRQ_STAT)
		. = ALIGN(4);
		KEEP(*(SW_ISR_TABLE))
#endif

#ifdef CONFIG_CUSTOM_RWDATA_LD
		/* Located in project source directory */
#include <custom-rwdata.ld>
#endif
	} GROUP_DATA_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)


	#include <linker/common-ram.ld>

	__data_ram_end = .;

	SECTION_DATA_PROLOGUE(BTCON_FIXBSS,(NOLOAD),)
	{
		. = ALIGN(4);
		__app_bss_start = .;
		/*no used */
		__app_bss_end = .;
#ifdef CONFIG_SOC_SERIES_ANDESC_P2
		*(.bthost.noinit.stack*)
#endif
		__btcon_host_bss_start = .;
		*(.bthost.bss*)
		*(.BT_MANAGER_BSS*)
		__btcon_host_bss_end = .;
	} GROUP_LINK_IN(ram_bt)

	SECTION_DATA_PROLOGUE(OTA_BSS,(NOLOAD),)
	{
		. = ALIGN(4);
		*(.otabss*)
	} GROUP_LINK_IN(ram_dsp)

	__mpu_ram_protect_start = .;

	SECTION_DATA_PROLOGUE(heap,(NOLOAD),)
	{
		. = ALIGN(4);
		__heap_start = .;
		/*no used */
		. = LENGTH(ram_mpool);
		__heap_end = .;
	} GROUP_LINK_IN(ram_mpool)

	SECTION_DATA_PROLOGUE(BTCON_ROM,,)
	{
		. = ALIGN(4);
		__btcon_ramfunc_start = .;
		/* BT controller fix code */
		KEEP(*(.BTCON_ROM_TEXT))
		KEEP(*(.BTCON_ROM_RODATA))

		__btcon_ramfunc_end = .;
	} GROUP_DATA_LINK_IN(ram_bt, ROMABLE_REGION)

	__mpu_ram_protect_end = .;

#ifndef CONFIG_AURACAST
	SECTION_DATA_PROLOGUE(WOOFER_BSS,(NOLOAD),)
	{
		. = ALIGN(4);
		*(.AUDIO_RESAMPLE2_RAM*)
		*(.MEDIA_DAE_BSS*)
                *(.AUDIO_SUBWOOFER_RAM*)
	} GROUP_LINK_IN(ram_dsp_free)
#endif

#ifdef CONFIG_SECTION_OVERLAY
	OVERLAY : NOCROSSREFS
	{
		.overlay.data.apfla {
			*lib_k1_fla_p.a:*(.data .data.*)
		}

		.overlay.data.apwma {
			*lib_k1_w13_p.a:*(.data .data.*)
		}

		.overlay.data.apape {
			*lib_k1_ape_p.a:*(.data .data.*)
		}

		.overlay.data.apmp3 {
			*lib_k1_mp3_p.a:*(.data .data.*)
		}

		.overlay.data.apwav {
			*lib_k1_wav_p.a:*(.data .data.*)
		}

		.overlay.data.apaac {
			*lib_k1_a13_p.a:*(.data .data.*)
		}
	} GROUP_DATA_LINK_IN(SRAM, ROMABLE_REGION)
#endif

	SECTION_DATA_PROLOGUE(_ACTDATA_SECTION_NAME,,)
	{
		*lib_k1_act_d.a:*(.data .data.*)
		*libresample.a:*(.data .data.*)
	} GROUP_DATA_LINK_IN(SRAM, ROMABLE_REGION)

	SECTION_DATA_PROLOGUE(_DSPBSS_SECTION_NAME,(NOLOAD),)
	{
		KEEP(*(SORT_BY_NAME(".decoderbss.*")))

#ifdef CONFIG_DSP_USE_HW_MEMORY
#ifdef CONFIG_AUDIO_MULTI_CHANNEL_MODE
		KEEP(*(SORT_BY_NAME(".subwooferbss.*")))
#endif
#endif
		KEEP(*(.decsbcbss.*))
		KEEP(*(.encsbcbss.*))
		*(".csbbss*")
	}GROUP_LINK_IN(SRAM)

	SECTION_DATA_PROLOGUE(_NAVBSS_SECTION_NAME,(NOLOAD),)
	{
		*(".navbss*")
	}GROUP_LINK_IN(SRAM)

	SECTION_DATA_PROLOGUE(_DSPSBCUNUSED_SECTION_NAME,(NOLOAD),)
{
#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
	KEEP(*lib_k1_act_d.a:*(.bss .bss.* .scommon COMMON))
	KEEP(*libdecencsbc.a:*(.bss .bss.* .scommon COMMON))
#endif

	}GROUP_LINK_IN(SRAM)

#ifdef CONFIG_SECTION_OVERLAY
	OVERLAY : NOCROSSREFS
	{
		.overlay.bss.share4 {
			__share4_media_ram_start = .;
				KEEP(*(.DSP_SHARE4_MEDIA_RAM*))
			__share4_media_ram_end = .;
		}

		.overlay.bss.snoop {
		/*only tws snoop*/
			KEEP(*(.BT_TWS_OBSERVER_BSS*))
		}
	} GROUP_DATA_LINK_IN(ram_dsp_free, ROMABLE_REGION)
#endif

	SECTION_DATA_PROLOGUE(_BSS_SECTION_NAME,(NOLOAD),)
	{
		/*
		* For performance, BSS section is assumed to be 4 byte aligned and
		* a multiple of 4 bytes
		*/
		. = ALIGN(4);
		__bss_start = .;

		__btcon_rom_bss_start = .;
		KEEP(*(.bss.puppet_data))
		__btcon_rom_bss_end = ALIGN(4);

	#ifdef CONFIG_SECTION_OVERLAY
		*(EXCLUDE_FILE (*libdecencsbc.a:* *libdeca13.a:* *libdecape.a:* *libdecflac.a:* *libdecamr.a:* *libdecwav.a:* *libdecmp3.a:* *libdecw13.a:* *lib_k1_act_d.a:* *libencamr.a:* *libwav.a:* *lib_k1_wav_p.a:* *lib_k1_w13_p.a:* *lib_k1_ape_p.a:* *lib_k1_mp3_p.a:* *lib_k1_fla_p.a:* *lib_k1_a13_p.a:* *libkshow.a:* *libencpp.a:* *libencopus.a:* *libencspeex.a:* *libdnoise.a:* *libCSpotter_16k24d.a:* *libresample.a:*) .bss)
		*(EXCLUDE_FILE (*libdecencsbc.a:* *libdeca13.a:* *libdecape.a:* *libdecflac.a:* *libdecamr.a:* *libdecwav.a:* *libdecmp3.a:* *libdecw13.a:* *lib_k1_act_d.a:* *libencamr.a:* *libwav.a:* *lib_k1_wav_p.a:* *lib_k1_w13_p.a:* *lib_k1_ape_p.a:* *lib_k1_mp3_p.a:* *lib_k1_fla_p.a:* *lib_k1_a13_p.a:* *libkshow.a:* *libencpp.a:* *libencopus.a:* *libencspeex.a:* *libdnoise.a:* *libCSpotter_16k24d.a:* *libresample.a:*) .bss.*)
		*(EXCLUDE_FILE (*libdecencsbc.a:* *libdeca13.a:* *libdecape.a:* *libdecflac.a:* *libdecamr.a:* *libdecwav.a:* *libdecmp3.a:* *libdecw13.a:* *lib_k1_act_d.a:* *libencamr.a:* *libwav.a:* *lib_k1_wav_p.a:* *lib_k1_w13_p.a:* *lib_k1_ape_p.a:* *lib_k1_mp3_p.a:* *lib_k1_fla_p.a:* *lib_k1_a13_p.a:* *libkshow.a:* *libencpp.a:* *libencopus.a:* *libencspeex.a:* *libdnoise.a:* *libCSpotter_16k24d.a:* *libresample.a:*) .scommon)
		*(EXCLUDE_FILE (*libdecencsbc.a:* *libdeca13.a:* *libdecape.a:* *libdecflac.a:* *libdecamr.a:* *libdecwav.a:* *libdecmp3.a:* *libdecw13.a:* *lib_k1_act_d.a:* *libencamr.a:* *libwav.a:* *lib_k1_wav_p.a:* *lib_k1_w13_p.a:* *lib_k1_ape_p.a:* *lib_k1_mp3_p.a:* *lib_k1_fla_p.a:* *lib_k1_a13_p.a:* *libkshow.a:* *libencpp.a:* *libencopus.a:* *libencspeex.a:* *libdnoise.a:* *libCSpotter_16k24d.a:* *libresample.a:*) COMMON)


		KEEP(*libdecencsbc.a:*(.bss .bss.* .scommon COMMON))
	#ifdef CONFIG_LCMUSIC_PLAYER_APP
		KEEP(*(SORT_BY_NAME(".decoderbss.*")))
	#endif
	#else
		*(.scommon)
		*(.bss)
		*(".bss.*")
		COMMON_SYMBOLS
	#endif
		/*
		* As memory is cleared in words only, it is simpler to ensure the BSS
		* section ends on a 4 byte boundary. This wastes a maximum of 3 bytes.
		*/
		. = ALIGN(16);
		__share_media_ram_start = .;
		KEEP(*(.DSP_SHARE_MEDIA_RAM*))
		__share_media_ram_end = .;

		KEEP(*(.DSP_INNER_RAM*))
		KEEP(*(.DSP_SHARE_RAM*))
		KEEP(*(.MBOX_MSGS_BSS*))
	#ifdef CONFIG_AURACAST
		*(.AUDIO_RESAMPLE2_RAM*)
		*(.MEDIA_DAE_BSS*)
		*(.AUDIO_SUBWOOFER_RAM*)
	#endif
		__bss_end = ALIGN(4);
	} GROUP_DATA_LINK_IN(RAMABLE_REGION, RAMABLE_REGION)



	SECTION_PROLOGUE(_NOINIT_SECTION_NAME,(NOLOAD),)
	{
		*(.bottom_of_image_ram)
		*(.bottom_of_image_ram.*)
	} GROUP_LINK_IN(RAMABLE_REGION)

#ifdef CONFIG_SECTION_OVERLAY
	OVERLAY : NOCROSSREFS
	{
		.overlay.bss.diskio {
			/*only use lcmusic*/
			KEEP(*(.diskio.cache.pool*))
			KEEP(*(.diskio.cache.stack*))
		}

		.overlay.bss.usbscan {
			/*only usb scan*/
			KEEP(*(.usb.scan.bss*))
		}

		.overlay.bss.usbscan {
			/*only usb scan*/
			KEEP(*(.disk_ota.scan.bss*))
		}

	} GROUP_DATA_LINK_IN(ram_usb_ms_buf, ROMABLE_REGION)
#endif

#ifdef CONFIG_SECTION_OVERLAY
	OVERLAY : NOCROSSREFS
	{
		.overlay.bss.apfla {
			*lib_k1_fla_p.a:*(.bss .bss.* .scommon COMMON)
		}

		.overlay.bss.apwma {
			*lib_k1_w13_p.a:*(.bss .bss.* .scommon COMMON)
		}

		.overlay.bss.apape {
			*lib_k1_ape_p.a:*(.bss .bss.* .scommon COMMON)
		}

		.overlay.bss.apmp3 {
			*lib_k1_mp3_p.a:*(.bss .bss.* .scommon COMMON)
		}

		.overlay.bss.apwav {
			*lib_k1_wav_p.a:*(.bss .bss.* .scommon COMMON)
		}

		.overlay.bss.apaac {
			*lib_k1_a13_p.a:*(.bss .bss.* .scommon COMMON)
		}
	} GROUP_LINK_IN(ram_usb_ms_buf)
#endif

	SECTION_PROLOGUE(_ACTBSS_SECTION_NAME,(NOLOAD),)
	{
		KEEP(*(SORT_BY_NAME(".actbss.*")))
		KEEP(*lib_k1_act_d.a:*(.bss .bss.* .scommon COMMON))
		*libresample.a:*(.bss .bss.* .scommon COMMON)
	} GROUP_LINK_IN(SRAM)


	SECTION_DATA_PROLOGUE(NOINIT,(NOLOAD),)
	{
		/*
		* This section is used for non-initialized objects that
		* will not be cleared during the boot process.
		*/
		*(.noinit)
		*(".noinit.*")
#ifndef CONFIG_SOC_SERIES_ANDESC_P2
		*(.bthost.noinit.stack*)
#endif

#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
	}GROUP_LINK_IN(PSRAM)
#else
	}GROUP_LINK_IN(SRAM)
#endif

	__share2_media_ram_start = ORIGIN(ram_usb_ms_buf);
	__share3_media_ram_start = ORIGIN(ram_parser_buf);

	SECTION_DATA_PROLOGUE(_PARSER_BSS_SECTION_NAME,(NOLOAD),)
	{
		*(.parserbss.*)
		*(.lcmusicbss.*)
		KEEP(*(.DSP_SHARE3_MEDIA_RAM*))
	} GROUP_LINK_IN(ram_parser_buf)

	SECTION_DATA_PROLOGUE(_ASRC_BSS_SECTION_NAME,(NOLOAD),)
	{
		KEEP(*(SORT_BY_NAME(".stackservicenoinit.*")))
	} GROUP_LINK_IN(ram_asrc0_buf)

	SECTION_DATA_PROLOGUE(_SRV_BSS_SECTION,(NOLOAD),)
	{
		KEEP(*(.AUDIO_TRACK_RELOAD_RAM*))
		KEEP(*(.MEDIA_CODEC_STACK_RAM*))
		KEEP(*(SORT_BY_NAME(".stacknoinit.*")))
                KEEP(*(SORT_BY_NAME(".ITERATER_RAM*")))
		*(.lcmusicfullname*)
	} GROUP_LINK_IN(ram_asrc1_buf)

	SECTION_DATA_PROLOGUE(_FILESELECTOR_BSS_SECTION_NAME,(NOLOAD),)
	{
		*(.lcmusicname.*)
	} GROUP_LINK_IN(ram_fileselector_buf)


	SECTION_DATA_PROLOGUE(_TRACE_PRINTBUF_SECTION_NAME,(NOLOAD),)
	{
		*(.traceprintbuf.*)
	} GROUP_LINK_IN(ram_trace_printbuf)

	/* Define linker symbols */
#ifdef CONFIG_APP_USED_MEM_PAGE
	__ram_mpool0_start = ORIGIN(ram_mpool);
	__ram_mpool0_size = LENGTH(ram_mpool);
	__ram_mpool0_num = (LENGTH(ram_mpool) + IBANK_SIZE - 1) / IBANK_SIZE;

	ASSERT((LENGTH(ram_mpool) == (__ram_mpool0_num * IBANK_SIZE)), "PAGE POOL SIZE must be alligned with IBANK_SIZE")
	ASSERT(!(ORIGIN(ram_mpool) & (IBANK_SIZE - 1)), "PAGE POOL start address must be alligned with IBANK_SIZE")
#endif

	__ram_dsp_start = ORIGIN(ram_dsp);
	__ram_dsp_size = LENGTH(ram_dsp);

	SECTION_DATA_PROLOGUE(RAM_FUNCTION,,)
	{
		. = ALIGN(4);
		_image_text_ramfunc_start = .;
		*(.top_of_image_ram)
		*(.top_of_image_ram.*)

		KEEP(*(SORT_BY_NAME(".coredata.*")))

		/* sections for function run in ram when XIP is enabled */
		KEEP(*(SORT_BY_NAME(".ramfunc.*")))

		_image_text_ramfunc_end = .;
	} GROUP_DATA_LINK_IN(ram_func, ROMABLE_REGION)


	SECTION_PROLOGUE(.RAM_ESD_DATA,(NOLOAD),) {
		*(.ESD_DATA*)
	} GROUP_LINK_IN(ram_esd_data)

	SECTION_PROLOGUE(.RAM_BOOTINFO_DATA,(NOLOAD),) {
		*(.BOOTINFO_DATA*)
	} GROUP_LINK_IN(ram_bootinfo)

	__ramfunc_ram_start = .;
	__ramfunc_ram_end = .;
	__ramfunc_rom_start = 0;

#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
	SECTION_DATA_PROLOGUE(.BTCON_MEMPOOL,(NOLOAD),) {
		*(.BTCON_MEMPOOL*)
		*(.BTCON_TO_HOST_BUF*)
	} GROUP_LINK_IN(BTCON_MEMPOOL_REGION)

	SECTION_DATA_PROLOGUE(.BTHOST_BSS,(NOLOAD),) {
		__btcon_host_buf_start = .;
		*(.bthost_bss*)
		KEEP(*(SORT_BY_NAME("._bt_buf_pool.noinit.*")))
		*(.btcon.noinit.stack*)
		__btcon_host_buf_end = .;
	} GROUP_LINK_IN(PSRAM)
#else
	SECTION_DATA_PROLOGUE(.BTCON_MEMPOOL,(NOLOAD),) {
		*(.BTCON_MEMPOOL*)
		*(.BTCON_TO_HOST_BUF*)
		__btcon_host_buf_start = .;
		*(.bthost_bss*)
		KEEP(*(SORT_BY_NAME("._bt_buf_pool.noinit.*")))
		*(.btcon.noinit.stack*)
		__btcon_host_buf_end = .;
	} GROUP_LINK_IN(BTCON_MEMPOOL_REGION)
#endif

	_image_ram_end = .;
	_end = .; /* end of image */

	GROUP_END(RAMABLE_REGION)

#ifdef CONFIG_CUSTOM_SECTIONS_LD
	/* Located in project source directory */
	#include <custom-sections.ld>
#endif

#ifdef CONFIG_GEN_ISR_TABLES
	#include <linker/intlist.ld>
#endif

	SECTION_DATA_PROLOGUE(ROM_END,,)
	{

	} GROUP_DATA_LINK_IN(ram_func, ROMABLE_REGION)

	 _image_flash_rom_size = LOADADDR(ROM_END) - _image_rom_start;
}

