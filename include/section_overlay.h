/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Sections overlay public API header file.
 */

#ifndef __INCLUDE_SECTION_OVERLAY_H__
#define __INCLUDE_SECTION_OVERLAY_H__

#define OVERLAY_TABLE_MAGIC	0x4c564f53

#define OVERLAY_ID_LIBADMP3			0x6433706d	/* "mp3" */
#define OVERLAY_ID_LIBADWMA			0x616d77	/* "wma" */
#define OVERLAY_ID_LIBADWAV			0x766177	/* "wav" */
#define OVERLAY_ID_LIBDECAMR		0x726d61	/* "amr" */
#define OVERLAY_ID_LIBADFLAC		0x63616c66	/* "flac" */
#define OVERLAY_ID_LIBDECAAC		0x636161	/* "aac" */
#define OVERLAY_ID_LIBDECLDAC
#define OVERLAY_ID_LIBADAPE			0x657061	/* "ape" */
#define OVERLAY_ID_LIBADACT			0x656774	/* "act" */
#define OVERLAY_ID_LIBDECM4A		0x61346d	/* "m4a" */

#define OVERLAY_ID_LIBENCAMR		0x454e43	/* "ENC" */
#define OVERLAY_ID_LIBSINVOICE		0x53494e56	/* "SINV" */
#define OVERLAY_ID_LIBSHOW			0x776f6873 	/* "show" */

#define OVERLAY_ID_LIBAPWAV			0x70766177	/* "wav p" */
#define OVERLAY_ID_LIBAPMP3         0x7033706d  /* "mp3 p" */
#define OVERLAY_ID_LIBAPAPE 		0x70337077  /* "ape p" */
#define OVERLAY_ID_LIBAPWMA 		0x70337088  /* "wma p" */
#define OVERLAY_ID_LIBAPFLAC		0x70337099	/* "fla p" */
#define OVERLAY_ID_LIBAPM4A			0x70613488	/* "m4a p"*/

#define OVERLAY_ID_LIBBTDRV		0x62746472	/* "btdr" */
#define OVERLAY_ID_LIBENCOPUS   0x4f505553  /* "opus" */
#define OVERLAY_ID_LIBPCMCONVERTER 0x70636d63  /* "pcmc" */
#define OVERLAY_ID_LIBENCPCM        0x6d454e43  /* "encp" */
#define OVERLAY_ID_LIBENCSPEEX     0x73706565  /* "speex" */
#define OVERLAY_ID_LIBDUER     0x62435074  /* "duer" */
#define OVERLAY_ID_LIBIFLY     0x796c6669  /* "ifly" */
#if !defined(_LINKER) && !defined(_ASMLANGUAGE)

int overlay_section_init(unsigned int idcode);
void overlay_section_dump(void);

#endif

#endif  /* __INCLUDE_SECTION_OVERLAY_H__ */
