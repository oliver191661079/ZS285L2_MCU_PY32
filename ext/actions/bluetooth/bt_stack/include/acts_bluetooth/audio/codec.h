/** @file
 *  @brief Generic Audio Codec
 */

/*
 * Copyright (c) 2020 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_CODEC_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_CODEC_H_

#include <toolchain.h>
#include <stddef.h>
#include <zephyr/types.h>
#include <misc/util.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
/*
 * Language types (2-byte is enough)
 */
#define BT_LANGUAGE_TYPE_UNKNOWN        0xFFFF
#define BT_LANGUAGE_TYPE_CHINESE        0x0000
#define BT_LANGUAGE_TYPE_ENGLISH        0x0001

/* 3-byte, lower case language code as defined in ISO 639-3 */
struct bt_language_code {
	uint8_t val[3];
};
#endif

/*
 * Language types (4-byte), append more items ...
 */
#if 0
#define BT_LANGUAGE_CHINESE        (('z' << 16) + ('h' << 8) + 'o')
#define BT_LANGUAGE_ENGLISH        (('e' << 16) + ('n' << 8) + 'g')
#else
#define BT_LANGUAGE_UNKNOWN        0x0
#define BT_LANGUAGE_CHINESE        ('z' + ('h' << 8) + ('o' << 16))
#define BT_LANGUAGE_ENGLISH        ('e' + ('n' << 8) + ('g' << 16))
#endif

/*
 * Audio Context types
 */
#define BT_AUDIO_CONTEXT_UNSPECIFIED          BIT(0)	/* mandatory */
#define BT_AUDIO_CONTEXT_CONVERSATIONAL       BIT(1)
#define BT_AUDIO_CONTEXT_MEDIA                BIT(2)
#define BT_AUDIO_CONTEXT_GAME                 BIT(3)
#define BT_AUDIO_CONTEXT_INSTRUCTIONAL        BIT(4)
#define BT_AUDIO_CONTEXT_VOICE_ASSISTANTS     BIT(5)
#define BT_AUDIO_CONTEXT_LIVE                 BIT(6)
#define BT_AUDIO_CONTEXT_SOUND_EFFECTS        BIT(7)
#define BT_AUDIO_CONTEXT_NOTIFICATIONS        BIT(8)
#define BT_AUDIO_CONTEXT_RINGTONE             BIT(9)
#define BT_AUDIO_CONTEXT_ALERTS               BIT(10)
#define BT_AUDIO_CONTEXT_EMERGENCY_ALARM      BIT(11)
#define BT_AUDIO_CONTEXT_MASK                 0x0FFF
#define BT_AUDIO_CONTEXT_ASQT_TEST            0xFFFF

/*
 * codec coding format: Assigned numbers and the Host Controller interface
 */
/* u-Law log */
#define BT_CODEC_CODING_FORMAT_ULAW	0x00
/* A-law log */
#define BT_CODEC_CODING_FORMAT_ALAW	0x01
/* CVSD */
#define BT_CODEC_CODING_FORMAT_CVSD	0x02
/* Transparent */
#define BT_CODEC_CODING_FORMAT_TP	0x03
/* Linear PCM */
#define BT_CODEC_CODING_FORMAT_LPCM	0x04
/* mSBC */
#define BT_CODEC_CODING_FORMAT_MSBC	0x05
/* LC3 */
#define BT_CODEC_CODING_FORMAT_LC3	0x06
/* Vendor Specific */
#define BT_CODEC_CODING_FORMAT_VS	0xFF

/*
 * metadata types
 */
#define BT_PREFERRED_AUDIO_CONTEXTS        0x01
#define BT_STREAMING_AUDIO_CONTEXTS        0x02
#define BT_PROGRAM_INFO_METADATA           0x03
#define BT_LANGUAGE_METADATA               0x04
#define BT_CCID_LIST_METADATA              0x05
#define BT_PARENTAL_RATING_METADATA        0x06
#define BT_PROGRAM_INFO_URI_METADATA       0x07
#define BT_EXTENDED_METADATA               0xFE
#define BT_VENDOR_SPECIFIC_METADATA        0xFF

/*
 *
 * Metadata field of PAC records in PACS
 */
struct bt_preferred_audio_contexts {
	uint8_t length;	/* 0x03 */
	uint8_t type;	/* BT_PREFERRED_AUDIO_CONTEXTS */
	uint16_t value;	/* bitfield: BT_AUDIO_CONTEXT_ */
} __packed;

/*
 * Streaming_Audio_Contexts
 *
 * 1. Enable or Update Metadata ASE Control operations for unicast Audio Streams
 * 2. included in a BASE structure for broadcast Audio Streams
 */
struct bt_streaming_audio_contexts {
	uint8_t length;	/* 0x03 */
	uint8_t type;	/* BT_STREAMING_AUDIO_CONTEXTS */
	uint16_t value;	/* bitfield: BT_AUDIO_CONTEXT_ */
} __packed;

/*
 *
 * Metadata field of Program_Info
 */
struct bt_program_info_metadata {
	uint8_t length;
	uint8_t type;	/* BT_PROGRAM_INFO_METADATA */
	uint8_t value[0];
} __packed;

/*
 *
 * Metadata field of Language
 */
struct bt_language_metadata {
	uint8_t length;	/* 0x04 */
	uint8_t type;	/* BT_LANGUAGE_METADATA */
	uint8_t value[3];	/* ISO 639-3 */
} __packed;

/*
 * Vendor_Specific Metadata
 *
 * 1. Enable or Update Metadata ASE Control operations for unicast Audio Streams
 * 2. included in a BASE structure for broadcast Audio Streams
 */
struct bt_vendor_specific_metadata {
	uint8_t length;	/* vary */
	uint8_t type;	/* BT_VENDOR_SPECIFIC_METADATA */
	/*
	 * value[0-1]: CID
	 * value[2]: Vendor-Specific Metadata Type
	 * value[3...]: Vendor-Specific Metadata
	 */
	uint8_t value[0];
} __packed;

/*
 * Audio Locations
 */
#define BT_AUDIO_LOCATIONS_NA       0	/* Not Allowed */
#define BT_AUDIO_LOCATIONS_FL       BIT(0)	/* Front Left */
#define BT_AUDIO_LOCATIONS_FR       BIT(1)	/* Front Right */
#define BT_AUDIO_LOCATIONS_FC       BIT(2)	/* Front Center */
#define BT_AUDIO_LOCATIONS_LFE1     BIT(3)	/* Low Frequency Effects 1 */
#define BT_AUDIO_LOCATIONS_BL       BIT(4)	/* Back Left */
#define BT_AUDIO_LOCATIONS_BR       BIT(5)	/* Back Right */
#define BT_AUDIO_LOCATIONS_FLC      BIT(6)	/* Front Left of Center */
#define BT_AUDIO_LOCATIONS_FRC      BIT(7)	/* Front Right of Center */
#define BT_AUDIO_LOCATIONS_BC       BIT(8)	/* Back Center */
#define BT_AUDIO_LOCATIONS_LFE2     BIT(9)	/* Low Frequency Effects 2 */
#define BT_AUDIO_LOCATIONS_SIL      BIT(10)	/* Side Left */
#define BT_AUDIO_LOCATIONS_SIR      BIT(11)	/* Side Right */
#define BT_AUDIO_LOCATIONS_TPFL     BIT(12)	/* Top Front Left */
#define BT_AUDIO_LOCATIONS_TPFR     BIT(13)	/* Top Front Right */
#define BT_AUDIO_LOCATIONS_TPFC     BIT(14)	/* Top Front Center */
#define BT_AUDIO_LOCATIONS_TPC      BIT(15)	/* Top Center */
#define BT_AUDIO_LOCATIONS_TPBL     BIT(16)	/* Top Back Left */
#define BT_AUDIO_LOCATIONS_TPBR     BIT(17)	/* Top Back Right */
#define BT_AUDIO_LOCATIONS_TPSIL    BIT(18)	/* Top Side Left */
#define BT_AUDIO_LOCATIONS_TPSIR    BIT(19)	/* Top Side Right */
#define BT_AUDIO_LOCATIONS_TPBC     BIT(20)	/* Top Back Center */
#define BT_AUDIO_LOCATIONS_BTFC     BIT(21)	/* Bottom Front Center */
#define BT_AUDIO_LOCATIONS_BTFL     BIT(22)	/* Bottom Front Left */
#define BT_AUDIO_LOCATIONS_BTFR     BIT(23)	/* Bottom Front Right */
#define BT_AUDIO_LOCATIONS_FLW      BIT(24)	/* Front Left Wide */
#define BT_AUDIO_LOCATIONS_FRW      BIT(25)	/* Front Right Wide */
#define BT_AUDIO_LOCATIONS_LS       BIT(26)	/* Left Surround */
#define BT_AUDIO_LOCATIONS_RS       BIT(27)	/* Right Surround */
#define BT_AUDIO_LOCATIONS_MAX      28
#define BT_AUDIO_LOCATIONS_MASK     0x0FFFFFFF

/*
 * LTV structure definitions
 */

/* Codec Specific Capabilities Types */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES        0x01
#define BT_SUPPORTED_FRAME_DURATIONS             0x02
#define BT_SUPPORTED_AUDIO_CHANNEL_COUNTS        0x03
#define BT_SUPPORTED_OCTETS_PER_CODEC_FRAME      0x04
#define BT_SUPPORTED_MAX_CODEC_FRAMES_PER_SDU    0x05
#define BT_SAMPLING_FREQUENCY                    0x01
#define BT_FRAME_DURATION                        0x02
#define BT_AUDIO_CHANNEL_ALLOCATION              0x03
#define BT_OCTETS_PER_CODEC_FRAME                0x04
#define BT_CODEC_FRAME_BLOCKS_PER_SDU            0x05

/*
 * Codec_Specific_Capabilities parameters
 *
 * for PAC records in PACS
 */

/* Supported_Sampling_Frequencies */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_8K      BIT(0)	/* 8000 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_11K     BIT(1)	/* 11025 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_16K     BIT(2)	/* 16000 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_22K     BIT(3)	/* 22050 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_24K     BIT(4)	/* 24000 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_32K     BIT(5)	/* 32000 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_44K     BIT(6)	/* 44100 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_48K     BIT(7)	/* 48000 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_88K     BIT(8)	/* 88200 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_96K     BIT(9)	/* 96000 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_176K    BIT(10)	/* 176400 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_192K    BIT(11)	/* 192000 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_384K    BIT(12)	/* 384000 */
#define BT_SUPPORTED_SAMPLING_FREQUENCIES_MASK    0x1FFF
struct bt_supported_sampling_frequencies {
	uint8_t length;	/* 0x03 */
	uint8_t type;	/* BT_SUPPORTED_SAMPLING_FREQUENCIES */
	uint16_t samples;	/* bitfield */
} __packed;

/* Supported_Frame_Durations */
#define BT_SUPPORTED_FRAME_DURATION_7_5MS      BIT(0)
#define BT_SUPPORTED_FRAME_DURATION_10MS       BIT(1)
#define BT_SUPPORTED_FRAME_DURATION_5MS        BIT(2)
#define BT_SUPPORTED_FRAME_DURATION_MASK       0x07
/* NOTICE: bit4 and bit5 are mutal exclusive */
#define BT_SUPPORTED_FRAME_DURATION_7_5MS_PREF BIT(4)
#define BT_SUPPORTED_FRAME_DURATION_10MS_PREF  BIT(5)
#define BT_SUPPORTED_FRAME_DURATION_5MS_PREF   BIT(6)
#define BT_SUPPORTED_FRAME_DURATION_PREF_MASK  0x70
struct bt_supported_frame_durations {
	uint8_t length;	/* 0x02 */
	uint8_t type;	/* BT_SUPPORTED_FRAME_DURATIONS */
	uint8_t durations;	/* bitfield */
} __packed;

/* Supported_Audio_Channel_Counts */
#define BT_AUDIO_CHANNEL_COUNTS_1    BIT(0)
#define BT_AUDIO_CHANNEL_COUNTS_2    BIT(1)
#define BT_AUDIO_CHANNEL_COUNTS_3    BIT(2)
#define BT_AUDIO_CHANNEL_COUNTS_4    BIT(3)
#define BT_AUDIO_CHANNEL_COUNTS_5    BIT(4)
#define BT_AUDIO_CHANNEL_COUNTS_6    BIT(5)
#define BT_AUDIO_CHANNEL_COUNTS_7    BIT(6)
#define BT_AUDIO_CHANNEL_COUNTS_8    BIT(7)
#define BT_AUDIO_CHANNEL_COUNTS_MAX  8
#define BT_AUDIO_CHANNEL_COUNTS_MASK 0xFF
struct bt_supported_audio_channel_counts {
	uint8_t length;	/* 0x02 */
	uint8_t type;	/* BT_SUPPORTED_AUDIO_CHANNEL_COUNTS */
	/*
	 * bitfield
	 * BIT(0): support 1 channel.
	 * BIT(1): support 2 channels.
	 * BIT(2): support 3 channels.
	 * BIT(0) & BIT(1): support 1 channel and 2 channels.
	 * BIT(1) & BIT(3): support 2 channels and 4 channels.
	 */
	uint8_t channels;
} __packed;

/* Supported_Octets_Per_Codec_Frame */
struct bt_supported_octets_per_codec_frame {
	uint8_t length;	/* 0x05 */
	uint8_t type;	/* BT_SUPPORTED_OCTETS_PER_CODEC_FRAME */
	uint16_t min_octets;
	uint16_t max_octets;
} __packed;

/* Supported_Max_Codec_Frames_Per_SDU */
struct bt_supported_max_codec_frames_per_sdu {
	uint8_t length;	/* 0x02 */
	uint8_t type;	/* BT_SUPPORTED_MAX_CODEC_FRAMES_PER_SDU */
	/*
	 * >= audio channel counts
	 * concatenate mulitple blocks: = audio channels * blocks
	 * if absence: = 1 codec frame per audio channel per SDU.
	 */
	uint8_t max_frames;
} __packed;

/*
 * Codec_Specific_Configuration parameters
 *
 * used for Config Codec or config a broadcast Audio Stream
 */

/* Sampling_Frequency */
#define BT_SAMPLING_FREQUENCY_8K      0x01
#define BT_SAMPLING_FREQUENCY_11K     0x02
#define BT_SAMPLING_FREQUENCY_16K     0x03
#define BT_SAMPLING_FREQUENCY_22K     0x04
#define BT_SAMPLING_FREQUENCY_24K     0x05
#define BT_SAMPLING_FREQUENCY_32K     0x06
#define BT_SAMPLING_FREQUENCY_44K     0x07
#define BT_SAMPLING_FREQUENCY_48K     0x08
#define BT_SAMPLING_FREQUENCY_88K     0x09
#define BT_SAMPLING_FREQUENCY_96K     0x0A
#define BT_SAMPLING_FREQUENCY_176K    0x0B
#define BT_SAMPLING_FREQUENCY_192K    0x0C
#define BT_SAMPLING_FREQUENCY_384K    0x0D

struct bt_sampling_frequency {
	uint8_t length;	/* 0x02 */
	uint8_t type;	/* BT_SAMPLING_FREQUENCY */
	uint8_t sample;
} __packed;

/* Frame_Duration */
#define BT_FRAME_DURATION_7_5MS   0x00
#define BT_FRAME_DURATION_10MS    0x01
#define BT_FRAME_DURATION_5MS     0x02
#define BT_FRAME_DURATION_MAX     0x02
#define BT_FRAME_DURATION_NA      0xFF	/* Not Allowed */

struct bt_frame_duration {
	uint8_t length;	/* 0x02 */
	uint8_t type;	/* BT_FRAME_DURATION */
	uint8_t duration;
} __packed;

/* BAP: B.4.2.3 Audio_Channel_Allocation */
struct bt_audio_channel_allocation {
	uint8_t length;	/* 0x05 */
	uint8_t type;	/* BT_AUDIO_CHANNEL_ALLOCATION */
	uint32_t locations;	/* bitfield: BT_AUDIO_LOCATIONS_ */
} __packed;

/* Octets_Per_Codec_Frame */
struct bt_octets_per_codec_frame {
	uint8_t length;	/* 0x03 */
	uint8_t type;	/* BT_OCTETS_PER_CODEC_FRAME */
	uint16_t octets;
} __packed;

/* Codec_Frame_Blocks_Per_SDU */
struct bt_codec_frame_blocks_per_sdu {
	uint8_t length;	/* 0x02 */
	uint8_t type;	/* BT_CODEC_FRAME_BLOCKS_PER_SDU */
	uint8_t blocks;
} __packed;

/** @} */

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_CODEC_H_ */
